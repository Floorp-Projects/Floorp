#include "string.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"

struct FilesTest
{
	FilesTest() : mConsole() {}

	int RunAllTests();
	
	void WriteStuff(nsOutputFileStream& s);
	int InputStream(const char* relativePath);
	int OutputStream(const char* relativePath);
	int IOStream(const char* relativePath);
	int Parent(const char* relativePath, nsNativeFileSpec& outParent);
	int Delete(nsNativeFileSpec& victim);
	int CreateDirectory(nsNativeFileSpec& victim);
	int IterateDirectoryChildren(nsNativeFileSpec& startChild);
	int CanonicalPath(const char* relativePath);

	void Banner(const char* bannerString);
	void Passed();
	void Failed();
	void Inspect();
		
	nsOutputFileStream mConsole;
};

//----------------------------------------------------------------------------------------
void FilesTest::Banner(const char* bannerString)
//----------------------------------------------------------------------------------------
{
	mConsole
		<< nsEndl
		<< "---------------------------" << nsEndl
		<< bannerString << " Test"       << nsEndl
		<< "---------------------------" << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::Passed()
//----------------------------------------------------------------------------------------
{
	mConsole << "Test passed." << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::Failed()
//----------------------------------------------------------------------------------------
{
	mConsole << "ERROR: Test failed." << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::Inspect()
//----------------------------------------------------------------------------------------
{
	mConsole << nsEndl << "^^^^^^^^^^ PLEASE INSPECT OUTPUT FOR ERRORS" << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::WriteStuff(nsOutputFileStream& s)
//----------------------------------------------------------------------------------------
{
	// Initialize a URL from a string without suffix.  Change the path to suit your machine.
	nsFileURL fileURL("file:///Development/MPW/MPW%20Shell", false);
	s << "File URL initialized to:     \"" << fileURL << "\""<< nsEndl;
	
	// Initialize a Unix path from a URL
	nsFilePath filePath(fileURL);
	s << "As a unix path:              \"" << (const char*)filePath << "\""<< nsEndl;
	
	// Initialize a native file spec from a URL
	nsNativeFileSpec fileSpec(fileURL);
	s << "As a file spec:               " << fileSpec << nsEndl;
	
	// Make the spec unique (this one has no suffix).
	fileSpec.MakeUnique();
	s << "Unique file spec:             " << fileSpec << nsEndl;
	
	// Assign the spec to a URL
	fileURL = fileSpec;
	s << "File URL assigned from spec: \"" << fileURL << "\""<< nsEndl;
	
	// Assign a unix path using a string with a suffix.
	filePath = "/Development/MPW/SysErrs.err";
	s << "File path reassigned to:     \"" << (const char*)filePath << "\""<< nsEndl;	
	
	// Assign to a file spec using a unix path.
	fileSpec = filePath;
	s << "File spec reassigned to:      " << fileSpec << nsEndl;
	
	// Make this unique (this one has a suffix).
	fileSpec.MakeUnique();
	s << "File spec made unique:        " << fileSpec << nsEndl;
} // WriteStuff

//----------------------------------------------------------------------------------------
int FilesTest::OutputStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true); // relative path.
	const char* pathAsString = (const char*)myTextFilePath;
	nsNativeFileSpec mySpec(myTextFilePath);
	{
		mConsole << "WRITING IDENTICAL OUTPUT TO " << pathAsString << nsEndl << nsEndl;
		nsOutputFileStream testStream(myTextFilePath);
		if (!testStream.is_open())
		{
			mConsole
			    << "ERROR: File "
			    << pathAsString
			    << " could not be opened for output"
			    << nsEndl;
			return -1;
		}
		FilesTest::WriteStuff(testStream);
	}	// <-- Scope closes the stream (and the file).

	if (!mySpec.Exists() || mySpec.IsDirectory() || !mySpec.IsFile())
	{
			mConsole
			    << "ERROR: File "
			    << pathAsString
			    << " is not a file (cela n'est pas un pipe)"
			    << nsEndl;
			return -1;
	}
	Passed();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::IOStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true); // relative path.
	const char* pathAsString = (const char*)myTextFilePath;
	mConsole
		<< "Replacing \"path\" by \"ZUUL\" in " << pathAsString << nsEndl << nsEndl;
	nsIOFileStream testStream(myTextFilePath);
	if (!testStream.is_open())
	{
		mConsole
		    << "ERROR: File "
		    << pathAsString
		    << " could not be opened for input+output"
		    << nsEndl;
		return -1;
	}
	char line[1000];
	testStream.seek(0); // check that the seek compiles
	while (!testStream.eof())
	{
		PRInt32 pos = testStream.tell();
		testStream.readline(line, sizeof(line));
		char* replacementSubstring = strstr(line, "path");
		if (replacementSubstring)
		{
			testStream.seek(pos + (replacementSubstring - line));
			testStream << "ZUUL";
			testStream.seek(pos); // back to the start of the line
		}
	}
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::InputStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true);
	const char* pathAsString = (const char*)myTextFilePath;
	mConsole << "READING BACK DATA FROM " << pathAsString << nsEndl << nsEndl;
	nsInputFileStream testStream2(myTextFilePath);
	if (!testStream2.is_open())
	{
		mConsole
		    << "ERROR: File "
		    << pathAsString
		    << " could not be opened for input"
		    << nsEndl;
		return -1;
	}
	char line[1000];
	
	testStream2.seek(0); // check that the seek compiles
	while (!testStream2.eof())
	{
		testStream2.readline(line, sizeof(line));
		mConsole << line << nsEndl;
	}
	Inspect();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Parent(
	const char* relativePath,
	nsNativeFileSpec& outParent)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true);
	const char* pathAsString = (const char*)myTextFilePath;
	nsNativeFileSpec mySpec(myTextFilePath);

    mySpec.GetParent(outParent);
    nsFilePath parentPath(outParent);
	mConsole
		<< "GetParent() on "
		<< "\n\t" << pathAsString
		<< "\n yields "
		<< "\n\t" << (const char*)parentPath
		<< nsEndl;
	Inspect();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Delete(nsNativeFileSpec& victim)
//----------------------------------------------------------------------------------------
{
	// - Test of non-recursive delete

    nsFilePath victimPath(victim);
	mConsole
		<< "Attempting to delete "
		<< "\n\t" << (const char*)victimPath
		<< "\n without recursive option (should fail)"	
		<< nsEndl;
	victim.Delete(false);	
	if (victim.Exists())
		Passed();
	else
	{
		mConsole
		    << "ERROR: File "
		    << "\n\t" << (const char*)victimPath
		    << "\n has been deleted without the recursion option,"
		    << "\n and is a nonempty directory!"
		    << nsEndl;
		return -1;
	}

	// - Test of recursive delete

	mConsole
		<< nsEndl
		<< "Deleting "
		<< "\n\t" << (const char*)victimPath
		<< "\n with recursive option"
		<< nsEndl;
	victim.Delete(true);
	if (victim.Exists())
	{
		mConsole
		    << "ERROR: Directory "
		    << "\n\t" << (const char*)victimPath
		    << "\n has NOT been deleted despite the recursion option!"
		    << nsEndl;
		return -1;
	}
	
	Passed();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::CreateDirectory(nsNativeFileSpec& dirSpec)
//----------------------------------------------------------------------------------------
{
    nsFilePath dirPath(dirSpec);
	mConsole
		<< "Testing CreateDirectory() using"
		<< "\n\t" << (const char*)dirPath
		<< nsEndl;
		
	dirSpec.CreateDirectory();
	if (dirSpec.Exists())
		Passed();
	else
	{
		Failed();
		return -1;
	}
	dirSpec.Delete(true);
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::IterateDirectoryChildren(nsNativeFileSpec& startChild)
//----------------------------------------------------------------------------------------
{
	// - Test of directory iterator

    nsNativeFileSpec grandparent;
    startChild.GetParent(grandparent); // should be the original default directory.
    nsFilePath grandparentPath(grandparent);
    
    mConsole << "Forwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator i(grandparent, +1); i; i++)
    {
    	char* itemName = ((nsNativeFileSpec&)i).GetLeafName();
    	mConsole << '\t' << itemName << nsEndl;
    	delete [] itemName;
    }

    mConsole << "Backwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator j(grandparent, -1); j; j--)
    {
    	char* itemName = ((nsNativeFileSpec&)j).GetLeafName();
    	mConsole << '\t' << itemName << nsEndl;
    	delete [] itemName;
    }
    Inspect();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::CanonicalPath(
	const char* relativePath)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true);
	const char* pathAsString = (const char*)myTextFilePath;
	if (*pathAsString != '/')
	{
		mConsole
		    << "ERROR: after initializing the path object with a relative path,"
		    << "\n the path consisted of the string "
		    << "\n\t" << pathAsString
		    << "\n which is not a canonical full path!"
		    << nsEndl;
		return -1;
	}
	Passed();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::RunAllTests()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{
	// Test of mConsole output
	
	mConsole << "WRITING TEST OUTPUT TO CONSOLE" << nsEndl << nsEndl;

	// Test of nsFileSpec
	
	Banner("Interconversion");
	WriteStuff(mConsole);
	Inspect();
		
	Banner("Canonical Path");
	if (CanonicalPath("mumble/iotest.txt") != 0)
		return -1;
	
	Banner("OutputStream");
	if (OutputStream("mumble/iotest.txt") != 0)
		return -1;
	
	Banner("InputStream");
	if (InputStream("mumble/iotest.txt") != 0)
		return -1;
	
	Banner("IOStream");
	if (IOStream("mumble/iotest.txt") != 0)
		return -1;
	if (InputStream("mumble/iotest.txt") != 0)
		return -1;
	
	Banner("Parent");
	nsNativeFileSpec parent;
	if (Parent("mumble/iotest.txt", parent) != 0)
		return -1;
		
	Banner("Delete");
	if (Delete(parent) != 0)
		return -1;
		
	Banner("CreateDirectory");
	if (CreateDirectory(parent) != 0)
		return -1;

	Banner("IterateDirectoryChildren");
	if (IterateDirectoryChildren(parent) != 0)
		return -1;

    return 0;
}

//----------------------------------------------------------------------------------------
int main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{

	FilesTest tester;
	return tester.RunAllTests();
} // main
