/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
//
// File:	HTMLMethodDump.h
// Author:	Simon Holmes a Court
//

#ifdef DEBUG_LOG
#include <fstream.h>

void dumpDissasembly(LogModuleObject &f, Uint8* start, Uint8* end);

//-----------------------------------------------------------------------------------------------------------
// Debugging Code for outputting HTML index
struct FileNode
{
	const char*	fileName;
	const char*	methodName;
	FileNode*	next;

	FileNode(const char* inFileName, const char* inMethodName) : 
		fileName(inFileName), methodName(inMethodName), next(NULL) {};
};

struct ClassNode
{
	const char*	className;
	FileNode*	files;	// pointers to files
	ClassNode*	next;

	ClassNode(const char* inClassName) : className(inClassName), files(NULL), next(NULL) {};
};

// Object to store references to all the HTML files generated

class FileIndex
{
private:
	ClassNode*	classes;

	FileIndex() : classes(NULL) {};
	ClassNode*	findClassName(const char* className);
	void		insertFileHelper(const char* fileName, const char* methodName, const char* className);
	void		outputHelper();
	// FIX no destructor -- memory leak

public:
//	static inline FileIndex& getFileIndex() { return (FileIndex::sFileIndex); };
	static FileIndex sFileIndex;

	static void	output() { FileIndex::sFileIndex.outputHelper(); }
	static void	insertFile(const char* f, const char* m, const char* c) { FileIndex::sFileIndex.insertFileHelper(f, m, c); }
};

//-----------------------------------------------------------------------------------------------------------
// MethodToHTML
UT_EXTERN_LOG_MODULE(MethodToHtml);

class MethodToHTML
{
public:
	LogModuleObject &mFile;

	MethodToHTML() : mFile(UT_LOG_MODULE(MethodToHtml)) {}

	// file stuff
	void openFile(const char* fileName);
	void closeFile();

	// html to begin and end a page
	void pageBegin(const char* name);
	void pageEnd();
	void heading(const char* label);

	// statistics
	void statsBegin();
	void statsLine(const char* label, const char* value);
	void statsLine(const char* label, const char* value1, const char* value2, const char* value3);
	void statsLine(const char* label, const int value);
	void bigstatsLine(const char* label, const char* value);
	void statsEnd();

	// dissassembly table
	void disassemblyTableBegin();
	void disassemblyRowBegin(Uint32 nodeNum);
	void disassemblyRowBegin(const char* label);
	void disassemblyColumnSeparator(Uint32 color);
	void disassemblyRowEnd();
	void disassemblyTableEnd();
};

#endif // DEBUG_LOG

