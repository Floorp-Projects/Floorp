#include "nsFileSpec.h"
#include <iostream>

#include "nsFileStream.h"

namespace FileTest
{
	void WriteStuff(ostream& s);
}

//----------------------------------------------------------------------------------------
void FileTest::WriteStuff(ostream& s)
//----------------------------------------------------------------------------------------
{
	// Initialize a URL from a string without suffix.  Change the path to suit your machine.
	nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
	s << "File URL initialized to:     \"" << (string&)fileURL << "\""<< endl;
	
	// Initialize a Unix path from a URL
	nsUnixFilePath filePath(fileURL);
	s << "As a unix path:              \"" << (string&)filePath << "\""<< endl;
	
	// Initialize a native file spec from a URL
	nsNativeFileSpec fileSpec(fileURL);
	s << "As a file spec:               " << fileSpec << endl;
	
	// Make the spec unique (this one has no suffix).
	fileSpec.MakeUnique();
	s << "Unique file spec:             " << fileSpec << endl;
	
	// Assign the spec to a URL
	fileURL = fileSpec;
	s << "File URL assigned from spec: \"" << (string&)fileURL << "\""<< endl;
	
	// Assign a unix path using a string with a suffix.
	filePath = "/Development/MPW/SysErrs.err";
	s << "File path reassigned to:     \"" << (string&)filePath << "\""<< endl;	
	
	// Assign to a file spec using a unix path.
	fileSpec = filePath;
	s << "File spec reassigned to:      " << fileSpec << endl;
	
	// Make this unique (this one has a suffix).
	fileSpec.MakeUnique();
	s << "File spec made unique:        " << fileSpec << endl;
} // WriteStuff

//----------------------------------------------------------------------------------------
void main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{

#if !defined(DEBUG) || (DEBUG==0)
#error "This test only works with a DEBUG build."
#endif
	// Test of nsFileSpec
	
	cout << "WRITING TEST OUTPUT TO cout" << endl << endl;
	FileTest::WriteStuff(cout);
	cout << endl << endl;
	
	// Test of nsOutputFileStream

	nsUnixFilePath myTextFilePath("/Development/iotest.txt");

	cout << "WRITING IDENTICAL OUTPUT TO " << myTextFilePath << endl << endl;
	nsOutputFileStream testStream(myTextFilePath);
	FileTest::WriteStuff(testStream);
		
	// Test of nsInputFileStream

	cout << "READING BACK DATA FROM " << myTextFilePath << endl << endl;
	nsInputFileStream testStream2(myTextFilePath);
	char line[1000];
	while (!testStream2.eof())
	{
		testStream2.getline(line, sizeof(line), '\n');
		cout << line << endl;
	}
		
} // main
