#include "songparser.hh"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <stdexcept>
#include <map>

/// @file
/// Functions used for parsing the StepMania SM format

namespace {
	
	// Here are some functions needed in reading the data.
	void assign(int& var, std::string const& str) {
		try {
			var = boost::lexical_cast<int>(str);
		} catch (...) {
			throw std::runtime_error("\"" + str + "\" is not valid integer value");
		}
	}
	void assign(double& var, std::string str) {
		std::replace(str.begin(), str.end(), ',', '.'); // Fix decimal separators
		try {
			var = boost::lexical_cast<double>(str);
		} catch (...) {
			throw std::runtime_error("\"" + str + "\" is not valid floating point value");
		}
	}
	void assign(bool& var, std::string const& str) {
		if (str == "YES" || str == "yes" || str == "1") var = true;
		else if (str == "NO" || str == "no" || str == "0") var = false;
		else throw std::runtime_error("Invalid boolean value: " + str);
	}
	int upper_case (int c) { return toupper (c); }
	int lower_case (int c) { return tolower (c); }
}



bool SongParser::smCheck(std::vector<char> const& data) {
	if (data[0] != '#' || data[1] < 'A' || data[1] > 'Z') return false;
	for (std::vector<char>::const_iterator it = data.begin(); it != data.end(); ++it)
		if (*it == ';') return true;
	return false;
}

/* Parsing the note data is separated into three different functions: smParse, smParseField and smParseNote.
- smParse only begins a loop which continues as long as there is something to read in the file. It also checks if the needed information
could be read.
- smParseField reads all data beginning with '#'. That is, all but the actual notes. This function calls smParseNotes every time it
reaches value #NOTES.
- smParseNotes reads the notes into vector called notes which is a vector of structs (Note);
*/

void SongParser::smParse() {
	Song& s = m_song;
	std::string line;
	// Parse the the entire file
	while (getline(line) && smParseField(line)) {}
	if (m_song.danceTracks.empty() ) throw std::runtime_error("No note data in the file");
	if (s.title.empty() || s.artist.empty()) throw std::runtime_error("Required header fields missing");
	std::string& music = s.music["background"];
	std::string tmp = s.path + "music.ogg";
	namespace fs = boost::filesystem;
	if ((music.empty() || !fs::exists(music)) && fs::exists(tmp)) music = tmp;
	// Convert stops to the format required in Song
	s.stops.resize(m_stops.size());
	for (std::size_t i = 0; i < m_stops.size(); ++i) s.stops[i] = stopConvert(m_stops[i]);
}
	
bool SongParser::smParseField(std::string line) {
	if (line.empty() || line == "\r") return true;
	if (line[0] == '/' && line[1] == '/') return true; //jump over possible comments
	if (line[0] == ';') return true;

	//Here the data contained by the current line is separated in key and value.
	//However, because of the differing format of notedata the value is analyzed only if key is not NOTES
	std::string::size_type pos = line.find(':');
	if (pos == std::string::npos) throw std::runtime_error("Invalid format, should be #key:value");
	std::string key = boost::trim_copy(line.substr(1, pos - 1));
	if (key == "NOTES") {
		
		/*All remaining data is parsed here.
			All five lines of note metadata is read first and then smParseNotes is called to read
			the actual note data.
			All data is read into m_song.danceTracks map container.
		*/

		while (getline(line)) {
		//<NotesType>:
			std::string notestype = boost::trim_copy(line.substr(0, line.find_first_of(':')));
			transform(notestype.begin(), notestype.end(), notestype.begin(), lower_case );
		//<Description>:
			if(!getline(line)) { throw std::runtime_error("Required note data missing"); }
			std::string description = boost::trim_copy(line.substr(0, line.find_first_of(':')));
		//<DifficultyClass>:
			if(!getline(line)) { throw std::runtime_error("Required note data missing"); }
			std::string difficultyclass = boost::trim_copy(line.substr(0, line.find_first_of(':')));
			transform(difficultyclass.begin(), difficultyclass.end(), difficultyclass.begin(), upper_case );
			DanceDifficulty danceDifficulty = DIFFICULTYCOUNT;
				if(difficultyclass == "BEGINNER") danceDifficulty = BEGINNER;
				if(difficultyclass == "EASY") danceDifficulty = EASY;
				if(difficultyclass == "MEDIUM") danceDifficulty = MEDIUM;
				if(difficultyclass == "HARD") danceDifficulty = HARD;
				if(difficultyclass == "CHALLENGE") danceDifficulty = CHALLENGE;

		//ignoring difficultymeter and radarvalues
			//<DifficultyMeter>:
			if(!getline(line)) { throw std::runtime_error("Required note data missing"); }
			if(!getline(line)) { throw std::runtime_error("Required note data missing"); }
			
		//<NoteData>:
			Notes notes = smParseNotes(line);

		//Here all note data from the current track is inserted into containers 
			DanceTrack danceTrack(description, notes);
			if (m_song.danceTracks.find(notestype) == m_song.danceTracks.end() ) {
				DanceDifficultyMap danceDifficultyMap;
				m_song.danceTracks.insert(std::make_pair(notestype, danceDifficultyMap));
			}
			m_song.danceTracks[notestype].insert(std::make_pair(danceDifficulty, danceTrack));
		}
			return false;
	}
	std::string value = boost::trim_copy(line.substr(pos + 1));
	//In case the value continues to several lines, all text before the ending character ';' is read to single line.
	while (value[value.size() -1] != ';') {
		std::string str;
		if (!getline (str)) throw std::runtime_error("Invalid format, semicolon missing after value of " + key);
		value += boost::trim_copy(str);
	}
	value = value.substr(0, value.size() - 1);	//Here the end character(';') is eliminated
	if (value.empty()) return true;
	if (key == "TITLE") m_song.title = value.substr(value.find_first_not_of(" :"));
	else if (key == "ARTIST") m_song.artist = value.substr(value.find_first_not_of(" "));
	else if (key == "BANNER") m_song.cover = value;
	else if (key == "MUSIC") m_song.music["background"] = m_song.path + value;
	else if (key == "BACKGROUND") m_song.background = value;
	else if (key == "OFFSET") { assign(m_gap, value); m_gap *= -1; }
	else if (key == "BPMS"){
			std::istringstream iss(value);
			double ts, bpm;	
			char chr;
			while (iss >> ts >> chr >> bpm) {
				if (ts == 0.0) m_bpm = bpm;
				addBPM(ts * 4.0, bpm);
				if (!(iss >> chr)) break;
			}
	}
	else if (key == "STOPS"){
			std::istringstream iss(value);
			double beat, sec;
			char chr;
			while (iss >> beat >> chr >> sec) {
				m_stops.push_back(std::make_pair(beat * 4.0, sec));
				if (!(iss >> chr)) break;
	
			}
	}
		/*.sm fileformat has also the following constants but they are ignored in this version of the parser:
		#SUBTITLE
		#TITLETRANSLIT
		#SUBTITLETRANSLIT
   		#ARTISTTRANSLIT
		#CREDIT
   		#CDTITLE
		#SAMPLESTART
    		#SAMPLELENGTH
		#SELECTABLE
		#BGCHANGE
		*/
	return true;
}




Notes SongParser::smParseNotes(std::string line) {
	//container for dance songs
	typedef std::map<int, Note> DanceChord;	//int indicates "arrow" position (cmp. fret in guitar) 
	typedef std::vector<DanceChord> DanceChords;

	DanceChords chords;	//temporary container for notes
	Notes notes;	
	unsigned measure = 1;
	double begin = 0.0;

	std::map<int, int> holdMarks; // Keeps track of hold notes not yet terminated

	while (getline(line)) {
		if (line.empty() || line == "\r") continue;
		if (line[0] == '/' && line[1] == '/') continue;
		if (line[0] == '#') return notes;
		if (line[0] == ',' || line[0] == ';') {
			double end = tsTime(measure * 16.0);
			unsigned div = chords.size();
			double step = (end - begin) / div;
			for (unsigned note = 0; note < div; ++note) {
				double t = begin + note * step;
				for (DanceChord::iterator it = chords[note].begin(), end = chords[note].end(); it != end; ++it) {
					int& holdIdx = holdMarks[it->first];  // holdIdx for current arrow
					Note& n = it->second;
					n.begin = n.end = t;
					// TODO: Proper LIFT handling
					if (n.type == Note::TAP || n.type == Note::MINE || n.type == Note::LIFT) notes.push_back(n);
					// TODO: Proper ROLL handling
					if (n.type == Note::HOLDBEGIN || n.type == Note::ROLL) {
						notes.push_back(n);  // Note added now, end time will be fixed later
						holdIdx = notes.size(); // Store index in notes plus one
						continue;
					}
					if (n.type == Note::HOLDEND) {
						if (holdIdx == 0) throw std::runtime_error("Hold end without beginning");
						notes[holdIdx - 1].end = t;
					}
					holdIdx = 0;
				}
			}
			chords.clear();
			begin = end;
			++measure;
			continue;
		}
		/*Note data is read into temporary container chords before
		finally reading it into vector notes. This is done so that the bpm and time stamp values
		would be easier to count afterwards.
		*/
		DanceChord chord;
		for(std::size_t i = 0; i < line.size(); i++) {
			char notetype = line[i];
			if (notetype == '0') continue;
			Note note;
			if(notetype == '1') note.type = Note::TAP;
			else if(notetype == '2') note.type = Note::HOLDBEGIN;
			else if(notetype == '3') note.type = Note::HOLDEND;
			else if(notetype == '4') note.type = Note::ROLL;
			else if(notetype == 'M') note.type = Note::MINE;
			else if(notetype == 'L') note.type = Note::LIFT;
			else if(notetype >= 'a' && notetype <= 'z') note.type = Note::TAP;
			else if(notetype >= 'A' && notetype <= 'Z') note.type = Note::TAP;
			else continue;
			note.note = i;
			chord[i] = note;
		}
		chords.push_back(chord);
	}
	//The code reaches here only when all data is read from the file.
	return notes;
}