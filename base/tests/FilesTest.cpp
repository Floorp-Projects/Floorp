#include "nsFileSpec.h"
#include "nsFileStream.h"

#ifdef NS_USING_NAMESPACE
#include <iostream>
	using namespace std;
#else
#include <iostream.h>
#endif

NS_NAMESPACE FileTest
{
	void WriteStuff(ostream& s);
} NS_NAMESPACE_END

//----------------------------------------------------------------------------------------
void FileTest::WriteStuff(ostream& s)
//----------------------------------------------------------------------------------------
{
	// Initialize a URL from a string without suffix.  Change the path to suit your machine.
	nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
	s << "File URL initialized to:     \"" << fileURL << "\""<< endl;
	
	// Initialize a Unix path from a URL
	nsFilePath filePath(fileURL);
	s << "As a unix path:              \"" << (const char*)filePath << "\""<< endl;
	
	// Initialize a native file spec from a URL
	nsNativeFileSpec fileSpec(fileURL);
	s << "As a file spec:               " << fileSpec << endl;
	
	// Make the spec unique (this one has no suffix).
	fileSpec.MakeUnique();
	s << "Unique file spec:             " << fileSpec << endl;
	
	// Assign the spec to a URL
	fileURL = fileSpec;
	s << "File URL assigned from spec: \"" << fileURL << "\""<< endl;
	
	// Assign a unix path using a string with a suffix.
	filePath = "/Development/MPW/SysErrs.err";
	s << "File path reassigned to:     \"" << (const char*)filePath << "\""<< endl;	
	
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

	nsFilePath myTextFilePath("/Development/iotest.txt");

	{
		cout << "WRITING IDENTICAL OUTPUT TO " << (const char*)myTextFilePath << endl << endl;
		nsOutputFileStream testStream(myTextFilePath);
		FileTest::WriteStuff(testStream);
	}	// <-- Scope closes the stream (and the file).

	// Test of nsInputFileStream

	{
		cout << "READING BACK DATA FROM " << (const char*)myTextFilePath << endl << endl;
		nsInputFileStream testStream2(myTextFilePath);
		char line[1000];
		
		testStream2.seekg(0); // check that the seek template compiles
		while (!testStream2.eof())
		{
			testStream2.getline(line, sizeof(line), '\n');
			cout << line << endl;
		}
	}
		
} // main
