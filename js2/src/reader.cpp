/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the JavaScript 2 Prototype.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.   Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the
* terms of the GNU Public License (the "GPL"), in which case the
* provisions of the GPL are applicable instead of those above.
* If you wish to allow use of your version of this file only
* under the terms of the GPL and not to allow others to use your
* version of this file under the NPL, indicate your decision by
* deleting the provisions above and replace them with the notice
* and other provisions required by the GPL.  If you do not delete
* the provisions above, a recipient may use your version of this
* file under either the NPL or the GPL.
*/

#include <algorithm>

#include "reader.h"

namespace JS = JavaScript;


// Create a Reader reading characters from the source string.
// sourceLocation describes the origin of the source and may be used for
// error messages. initialLineNum is the line number of the first line of the
// source string.
JS::Reader::Reader(const String &source, const String &sourceLocation, uint32 initialLineNum):
        source(source + uni::null), sourceLocation(sourceLocation), initialLineNum(initialLineNum)
{
    begin = p = this->source.data();
    end = begin + this->source.size() - 1;
#ifdef DEBUG
    recordString = 0;
#endif
    beginLine();
}


// Mark the beginning of a line.  Call this after reading every line break to
// fill out the line start table.
void JS::Reader::beginLine()
{
    ASSERT(p <= end && (!linePositions.size() || p > linePositions.back()));
    linePositions.push_back(p);
}

// Fully process the source in order to fill in the line start table.
void JS::Reader::fillLineStartsTable()
{
    char16 ch;
    do {
        ch = get();
        if (isLineBreak(ch))
            beginLine();        
    } while (!getEof(ch));
}

// Return the number of the line containing the given character position.
// The line starts should have been recorded by calling beginLine.
uint32 JS::Reader::posToLineNum(size_t pos) const
{
    ASSERT(pos <= getPos());
    std::vector<const char16 *>::const_iterator i =
        std::upper_bound(linePositions.begin(), linePositions.end(), begin + pos);
    ASSERT(i != linePositions.begin());

    return static_cast<uint32>(i-1 - linePositions.begin()) + initialLineNum;
}


// Return the character position as well as pointers to the beginning and end
// (not including the line terminator) of the nth line.  If lineNum is out of
// range, return 0 and two nulls. The line starts should have been recorded by
// calling beginLine().  If the nth line is the last one recorded, then getLine
// manually finds the line ending by searching for a line break; otherwise,
// getLine assumes that the line ends one character before the beginning
// of the next line.
size_t JS::Reader::getLine(uint32 lineNum, const char16 *&lineBegin, const char16 *&lineEnd) const
{
    lineBegin = 0;
    lineEnd = 0;
    if (lineNum < initialLineNum)
        return 0;
    lineNum -= initialLineNum;
    if (lineNum >= linePositions.size())
        return 0;
    lineBegin = linePositions[lineNum];

    const char16 *e;
    ++lineNum;
    if (lineNum < linePositions.size())
        e = linePositions[lineNum] - 1;
    else {
        e = lineBegin;
        const char16 *end = Reader::end;
        while (e != end && !isLineBreak(*e))
            ++e;
    }
    lineEnd = e;
    return toSize_t(lineBegin - begin);
}


// Begin accumulating characters into the recordString, whose initial value is
// ignored and cleared.  Each character passed to recordChar() is added to the
// end of the recordString.  Recording ends when endRecord() or beginLine()
// is called. Recording is significantly optimized when the characters passed
// to readChar() are the same characters as read by get().  In this case the
// recorded String does not get allocated until endRecord() is called or a
// discrepancy appears between get() and recordChar().
void JS::Reader::beginRecording(String &recordString)
{
    Reader::recordString = &recordString;
    recordBase = p;
    recordPos = p;
}


// Append ch to the recordString.
void JS::Reader::recordChar(char16 ch)
{
    ASSERT(recordString);
    if (recordPos) {
        if (recordPos != end && *recordPos == ch) {
            recordPos++;
            return;
        } else {
            recordString->assign(recordBase, recordPos);
            recordPos = 0;
        }
    }
    *recordString += ch;
}


// Finish recording characters into the recordString that was last passed to
// beginRecording(). Return that recordString.
JS::String &JS::Reader::endRecording()
{
    String *rs = recordString;
    ASSERT(rs);
    if (recordPos)
        rs->assign(recordBase, recordPos);
    recordString = 0;
    return *rs;
}


// Report an error at the given character position in the source code.
void JS::Reader::error(Exception::Kind kind, const String &message, size_t pos)
{
    uint32 lineNum = posToLineNum(pos);
    const char16 *lineBegin;
    const char16 *lineEnd;
    size_t linePos = getLine(lineNum, lineBegin, lineEnd);
    ASSERT(lineBegin && lineEnd && linePos <= pos);

    throw Exception(kind, message, sourceLocation, lineNum, pos - linePos, pos, lineBegin, lineEnd);
}


// Read a line from the input file, including the trailing line break character.
// Return the total number of characters read, which is str's length.
// Translate <CR> and <CR><LF> sequences to <LF> characters; a <CR><LF> sequence
// only counts as one character.
size_t JS::LineReader::readLine(string &str)
{
    int ch;
    bool oldCRWasLast = crWasLast;
    crWasLast = false;

    str.resize(0);
#ifdef XP_UNIX
    char line[256];
    if (fgets(line, sizeof line, file) == NULL)
        return 0;
    str = line;
#else    
    while ((ch = getc(in)) != EOF) {
        if (ch == '\n') {
            if (!str.size() && oldCRWasLast)
                continue;
            str += '\n';
            break;
        }
        if (ch == '\r') {
            crWasLast = true;
            str += '\n';
            break;
        }
        str += static_cast<char>(ch);
    }
#endif
    return str.size();
}


// Same as readLine of a string except that widens the resulting characters to Unicode.
size_t JS::LineReader::readLine(String &wstr)
{
    string str;
    size_t n = readLine(str);
    wstr.resize(n);
    std::transform(str.begin(), str.end(), wstr.begin(), widen);
    return n;
}
