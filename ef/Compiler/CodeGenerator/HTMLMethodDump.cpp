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
// File:	HTMLMethodDump.cpp
// Author:	Simon Holmes a Court
//

#include "Fundamentals.h"

#ifdef DEBUG_LOG

#include "LogModule.h"
#include "HTMLMethodDump.h"
#include "NativeFormatter.h"
#include "FieldOrMethod.h"
#include "ExceptionTable.h"
#include "ControlGraph.h"
#include <time.h>

//-----------------------------------------------------------------------------------------------------------
// Native Formatter method
UT_DEFINE_LOG_MODULE(MethodToHtml);

void dumpDissasembly(LogModuleObject &f, Uint8* start, Uint8* end)
{
	if (end <= start)	// this happens for the last node, or nodes without code
		return;		

	Uint8* curAddr = start;
	while ((curAddr != NULL) && (curAddr < end))
		curAddr = (Uint8*) disassemble1(f, curAddr);
}

static void dumpHTMLByteCode(MethodToHTML output, Method* inMethod)
{
	UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("\n<br>\n"));
	output.heading("ByteCode");
	UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("\n<br><blockquote>\n<pre>\n"));
	inMethod->dumpBytecodeDisassembly(output.mFile);
	UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("</pre></blockquote>\n"));
}

static void dumpHTMLExceptionTable(MethodToHTML output, ExceptionTable& inExceptionTable, ControlNode** nodes, Uint32 nNodes)
{
	UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("\n<br>\n"));
	output.heading("Exception Table");
	UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("\n<br><blockquote>\n<pre>"));
	inExceptionTable.print(output.mFile);
	UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("</pre></blockquote>\n"));

	inExceptionTable.printFormatted(output.mFile, nodes, nNodes, true);
}

void NativeFormatter::
dumpMethodToHTML(FormattedCodeInfo& fci, ExceptionTable& inExceptionTable)
{
	// create and prepare the output file
	MethodToHTML output;
	output.mFile.setLogLevel(PR_LOG_ALWAYS);
	output.mFile.setOptions(PR_LOG_OPTION_NIL);

	assert(fci.method);

#if defined(XP_PC) || defined(LINUX)
	if (*fci.methodStart == 0xcc)
		fci.methodStart++;
#endif

	// get clean name
	const char* oldname = fci.method->getName();
	char* name = new char[strlen(oldname) + 1];
	strcpy(name, oldname);
	char* q = name;
	while(*q)
	{
		switch(*q)
		{
		case ':': case '<': case '>': case '/': case ' ':
			*q = '_';
		}
		q++;
	}

	const char* filename = fci.method->getHTMLName();

	// open file a clean filename
	output.openFile(filename);	
	output.pageBegin(name);

	// stats
	const char* className = fci.method->getDeclaringClass()->getName();

	output.statsBegin();
	output.bigstatsLine("Name", name);
	output.statsLine("Class/Name/Sig", className, name, fci.method->getSignatureString());
	output.statsLine("Fully qualified name", fci.method->toString());
	output.statsLine("HTML File Name", filename);

	output.statsLine("Native Code Bytes", fci.methodEnd - fci.methodStart);

	time_t currentTime;	// output the time
	time(&currentTime);
	output.statsLine("Created", ctime(&currentTime));

	output.statsEnd();

	// disassemble bytecode
	dumpHTMLByteCode(output, fci.method);

	// disassemble
	output.heading("Generated Code");
	output.disassemblyTableBegin();    
 
	// dump prolog
	Uint8*  curOffset;
    curOffset = fci.methodStart;
    output.disassemblyRowBegin("Pre");
	dumpDissasembly(output.mFile, curOffset, curOffset + fci.preMethodSize);
    curOffset += fci.preMethodSize;
	output.disassemblyColumnSeparator(0xeeeeee);
	output.disassemblyColumnSeparator(0xdddddd);
	output.disassemblyRowEnd();

	output.disassemblyRowBegin("Pro");
	dumpDissasembly(output.mFile, curOffset, curOffset + fci.prologSize);
    curOffset += fci.prologSize;
	output.disassemblyColumnSeparator(0xeeeeee);
	output.disassemblyColumnSeparator(0xdddddd);
	output.disassemblyRowEnd();

	// dump methods
	nodes[0]->controlGraph.dfsSearch();
	nodes[0]->controlGraph.assignProducerNumbers(1);

	// now go through and print out the high level representation (w/o prolog etc)
	for (Uint32 n = 0; n < nNodes; n++)
	{
		ControlNode& node = *nodes[n];
		output.disassemblyRowBegin(node.dfsNum);

		// actual disassembly
		Uint8* nodeStart;
		Uint8* nodeEnd;

		nodeStart = curOffset;

		if (n != (nNodes - 1))
			nodeEnd = fci.methodStart + nodes[n+1]->getNativeOffset();
		else // last node
			nodeEnd = fci.methodEnd;

        curOffset = nodeEnd;

		dumpDissasembly(output.mFile, nodeStart, nodeEnd);
		output.disassemblyColumnSeparator(0xeeeeee);

		// intended disassembly
		InstructionList& instructions = node.getInstructions();
		for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i))
		{
			instructions.get(i).printPretty(output.mFile);
			UT_OBJECTLOG(output.mFile, PR_LOG_ALWAYS, ("\n"));
		}

		// primitive graph
		output.disassemblyColumnSeparator(0xdddddd);
		node.printPretty(output.mFile, 0);

		output.disassemblyRowEnd();
	}

	output.disassemblyTableEnd();

	// exception table
	dumpHTMLExceptionTable(output, inExceptionTable, nodes, nNodes);

	output.pageEnd();
	output.closeFile();

	// add this file reference to the index file
	FileIndex::insertFile(filename, name, className);
	FileIndex::output();					// FIX FIX FIX for now we output this file EVERY time we generate code
}

//-----------------------------------------------------------------------------------------------------------
FileIndex FileIndex::sFileIndex;

void FileIndex::
outputHelper()
{
	// open the index file
	FILE* f = fopen("index.html","w");
	assert(f);

	// print the header
	fprintf(f,	"<HTML>\n<HEAD>\n<TITLE>Generated Code Index</TITLE>\n</HEAD>\n"
				"<BODY TEXT=#000000 BGCOLOR=#FFFFFF LINK=#FF0000 VLINK=#800080 ALINK=#0000FF>\n\n" );

	// output links
	ClassNode* c = classes;
	while(c)
	{
		fprintf(f, "\n<br><B><FONT FACE=\"Arial,Helvetica\">%s</FONT></B><br>\n", c->className);
		FileNode* n = c->files;
		while(n)
		{
			fprintf(f, "\t<A HREF=\"%s\">%s</A><br>\n", n->fileName, n->methodName);
			n = n->next;
		}
		c = c->next;
	}

	// output footers
	fprintf(f, "\n</BODY>\n</HTML>\n");
	fclose(f);
}

ClassNode* FileIndex::
findClassName(const char* className)
{
	if(classes == NULL)
	{
		classes = new ClassNode(className);
		return classes;
	}

	// search for a class object if there is one
	ClassNode* foundClass = classes;
	while(true)
	{
		if(strncmp(foundClass->className, className, 512) == 0)
			return foundClass;
		
		// no match, if end, make new node
		if(foundClass->next == NULL)
		{
			foundClass->next = new ClassNode(className);
			return foundClass->next;
		}				
		else
			foundClass = foundClass->next;
	}
}

void FileIndex::
insertFileHelper(const char* inFileName, const char* inMethodName, const char* inClassName)
{
	ClassNode* classnode = findClassName(inClassName);
	FileNode* firstNode = classnode->files;
	FileNode* newNode = new FileNode(inFileName, inMethodName);
	
	if(firstNode == NULL)
	{
		classnode->files = newNode;
		return;
	}

	// is it before the first?
	if(strncmp(inMethodName, firstNode->methodName, 512) < 0)
	{
		classnode->files = newNode;
		newNode->next = firstNode;
		return;
	}

	// otherwise iterate through until we are after a node and then insert
	FileNode* node = firstNode;
	while(true)
	{
		if(	node->next == NULL ||										// at end
			(strncmp(inMethodName, node->next->methodName, 512) < 0) )	// lexically before next name
		{
			FileNode* tempNode = node->next;
			node->next = newNode;
			newNode->next = tempNode;
			return;
		}
		node = node->next;
	}
}

//-----------------------------------------------------------------------------------------------------------
// Debugging Code for outputting Exception Tables
static void tableBegin(LogModuleObject &f, bool isHTML)
{
	if(isHTML) 
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<BLOCKQUOTE>\n<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=0>\n"));
}
static void tableEnd(LogModuleObject &f, bool isHTML)
{
	isHTML ? UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("</TABLE>\n</BLOCKQUOTE>\n")) : UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n\n\n"));
}
static void heading(LogModuleObject &f, Uint32 num, bool isHTML)
{
	if(isHTML)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TD WIDTH=20 BGCOLOR=#5b88f0 ALIGN=CENTER><B><FONT FACE=\"Arial,Helvetica\">%d</FONT></B></TD>", num));
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" %02d ", num));
}
static void rowBegin(LogModuleObject &f, bool isHTML)
{
	if(isHTML)
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TR><TD BGCOLOR=#5b88f0 WIDTH=50><B><FONT FACE=\"Arial,Helvetica\">Node</TD>"));
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TD BGCOLOR=#5b88f0 WIDTH=50><B><FONT FACE=\"Arial,Helvetica\">Offset</TD>"));
	}
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("N        "));
}
static void nodeRowBegin(LogModuleObject &f, Uint32 node, Uint32 offset, bool isHTML)
{
	if(isHTML)
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TR><TD BGCOLOR=#eeeeee><B><FONT FACE=\"Arial,Helvetica\">N%d</TD>", node));
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TD ALIGN=RIGHT BGCOLOR=#dddddd>%d </FONT></B></TD>", offset));
	}
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("N%02d: %4d  ", node, offset));
}
static void rowEnd(LogModuleObject &f, bool isHTML)
{
	isHTML ? UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("</TR>\n")) : UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
}
static void printInside(LogModuleObject &f, bool isHTML)
{
	isHTML ? UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TD BGCOLOR=#ff0000>&nbsp;</TD>")) :	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("|   "));
}
static void printOutside(LogModuleObject &f, Uint32 col, bool isHTML)
{
	if(isHTML)
		if(col&1)
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TD BGCOLOR=#dddddd>&nbsp;</TD>"));
		else
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<TD BGCOLOR=#eeeeee>&nbsp;</TD>"));
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("    "));
}

void ExceptionTable::
printFormatted(LogModuleObject &f, ControlNode** inNodes, Uint32 numNodes, bool isHTML)
{	
	Uint32 col;
	if(numberofEntries == 0)
		return;

	// print headers
	tableBegin(f, isHTML);
	rowBegin(f, isHTML);
	for(col = 0; col < numberofEntries; col++)
		heading(f, col, isHTML);
	rowEnd(f, isHTML);

	// print table
	for (Uint32 i = 0; i < numNodes; i++)
	{
		ControlNode* node = inNodes[i];
		Uint32 pc = node->getNativeOffset();
		nodeRowBegin(f, node->dfsNum, pc, isHTML);

		for(col = 0; col < numberofEntries; col++)
			if (pc >= mEntries[col].pStart && pc < mEntries[col].pEnd)
				printInside(f, isHTML);
			else
				printOutside(f, col, isHTML);

		rowEnd(f, isHTML);
	}
	tableEnd(f, isHTML);
}

//-----------------------------------------------------------------------------------------------------------
// Debugging Code for outputting HTML pages

void MethodToHTML::
openFile(const char* fileName)
{
	// open the file
	if (!mFile.setLogFile(fileName))
		trespass("opening log file failed");
}

void MethodToHTML::
closeFile()
{
    mFile.flushLogFile();
//	fclose(mFile);
}

void MethodToHTML::
pageBegin(const char* name)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<HTML>\n<HEAD>\n<TITLE>%s</TITLE>\n</HEAD>\n", name));
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<BODY TEXT=#000000 BGCOLOR=#FFFFFF LINK=#FF0000 VLINK=#800080 ALINK=#0000FF>\n\n"));
}

// Heading
void MethodToHTML::
heading(const char* label)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<B><FONT SIZE=+1 FACE=\"Arial,Helvetica\">%s</FONT></B><BR>\n", label));
}

// Stats
void MethodToHTML::
statsBegin()
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("\n\n<FONT SIZE=+2>\n<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3 COLS=2>\n"));
}

void MethodToHTML::
statsLine(const char* label, const char* value)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TR><TD WIDTH=\"200\"><B>%s</B></TD><TD>%s</TD></TR>\n", label, value));
}

void MethodToHTML::
bigstatsLine(const char* label, const char* value)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TR>\n<TD WIDTH=\"200\"><B><FONT SIZE=+1 FACE=\"Arial,Helvetica\">%s</FONT></B></TD>\n"
					"<TD><B><FONT SIZE=+1 FACE=\"Arial,Helvetica\">%s</FONT></B></TD>\n</TR>\n", label, value));
}

void MethodToHTML::
statsLine(const char* label, const int value)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TR><TD><B>%s</B></TD><TD>%d</TD></TR>\n", label, value));
}

void MethodToHTML::
statsLine(const char* label, const char* value1, const char* value2, const char* value3)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TR><TD WIDTH=\"200\"><B>%s</B></TD><TD>%s %s %s</TD></TR>\n", label, value1, value2, value3));
}


void MethodToHTML::
statsEnd()
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("</TABLE>\n</FONT>\n<BR>&nbsp;\n<BR>&nbsp;\n\n<A HREF=\"index.html\">go to index</A>\n<BR>&nbsp;\n<BR>&nbsp;\n\n"));
}

// disassembly
void MethodToHTML::
disassemblyTableBegin()
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=3 COLS=4 WIDTH=\"95%%\">\n<TR BGCOLOR=#5b88f0>\n"));
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TD WIDTH=50><CENTER><B><FONT FACE=\"Arial,Helvetica\">Node</FONT></B></CENTER></TD>\n"));
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TD><CENTER><B><FONT FACE=\"Arial,Helvetica\">Disassembly</FONT></B></CENTER></TD>\n"));
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TD><CENTER><B><FONT FACE=\"Arial,Helvetica\">Intended Code</FONT></B></CENTER></TD>\n"));
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("<TD><CENTER><B><FONT FACE=\"Arial,Helvetica\">Primitives</FONT></B></CENTER></TD>\n</TR>" ));
}

void MethodToHTML::
disassemblyRowBegin(Uint32 nodeNum)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("\n\n\n<TR>\n<TD ALIGN=CENTER VALIGN=TOP BGCOLOR=#eeeeee><A NAME=\"N%d\"><B><FONT FACE=\"Arial,Helvetica\">N%d</FONT></B></A>"
					"</TD>\n<TD VALIGN=TOP BGCOLOR=#dddddd><PRE>", nodeNum, nodeNum));
}

void MethodToHTML::
disassemblyRowBegin(const char* label)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("\n\n\n<TR>\n<TD ALIGN=CENTER VALIGN=TOP BGCOLOR=#eeeeee><A NAME=\"%s\"><B><FONT FACE=\"Arial,Helvetica\">%s</FONT></B></A>"
					"</TD>\n<TD VALIGN=TOP BGCOLOR=#dddddd><PRE>", label, label));
}

void MethodToHTML::
disassemblyColumnSeparator(Uint32 color)
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("&nbsp;</PRE></TD>\n\n<TD VALIGN=TOP BGCOLOR=#%06x><PRE>", color));
}

void MethodToHTML::
disassemblyRowEnd()
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("&nbsp;</PRE></TD>\n</TR>\n\n"));
}

void MethodToHTML::
disassemblyTableEnd()
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("</TABLE>\n\n"));
}

void MethodToHTML::
pageEnd()
{
	UT_OBJECTLOG(mFile, PR_LOG_ALWAYS, ("\n</BODY>\n</HTML>\n"));
}

//-----------------------------------------------------------------------------------------------------------

#endif // DEBUG_LOG
