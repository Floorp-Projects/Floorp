/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef reader_h___
#define reader_h___

#include <vector>
#include <cstdio>

#include "utilities.h"
#include "exception.h"

namespace JavaScript {

    // A Reader reads Unicode characters from a source string.
    // Calling get() yields all of the characters in sequence.
    // One character may be read
    // past the end of the source; that character appears as a null.
    class Reader {
        const char16 *begin;            // Beginning of source text
        const char16 *p;                // Position in source text
        const char16 *end;              // End of source text; *end is a null character
      public:
        const String source;            // Source text
        const String sourceLocation;    // Description of location from which the source text came
      private:
        const uint32 initialLineNum;    // One-based number of current line
        std::vector<const char16 *> linePositions; // Array of line starts recorded by beginLine()

        String *recordString;           // String, if any, into which recordChar() records characters; not owned by the Reader
        const char16 *recordBase;       // Position of last beginRecording() call
        const char16 *recordPos;        // Position of last recordChar() call; nil if a discrepancy occurred
      public:
        Reader(const String &source, const String &sourceLocation, uint32 initialLineNum = 1);
      private:
        Reader(const Reader&);          // No copy constructor
        void operator=(const Reader&);  // No assignment operator
      public:

        char16 get() {ASSERT(p <= end); return *p++;}
        char16 peek() {ASSERT(p <= end); return *p;}
        void unget(size_t n = 1) {ASSERT(p >= begin + n); p -= n;}
        size_t getPos() const {return toSize_t(p - begin);}
        void setPos(size_t pos) {ASSERT(pos <= getPos()); p = begin + pos;}

        bool eof() const {ASSERT(p <= end); return p == end;}
        bool peekEof(char16 ch) const {
            // Faster version.  ch is the result of a peek
            ASSERT(p <= end && *p == ch);
            return !ch && p == end;
        }
        bool pastEof() const {return p == end+1;}
        bool getEof(char16 ch) const {
            // Faster version.  ch is the result of a get
            ASSERT(p[-1] == ch); return !ch && p == end+1;
        }
        void beginLine();
        void fillLineStartsTable();
        uint32 posToLineNum(size_t pos) const;
        size_t getLine(uint32 lineNum, const char16 *&lineBegin, const char16 *&lineEnd) const;
        void beginRecording(String &recordString);
        void recordChar(char16 ch);
        String &endRecording();

        void error(Exception::Kind kind, const String &message, size_t pos);
    };


    class LineReader {
        FILE *in;                       // File from which currently reading
        bool crWasLast;                 // True if a CR character was the last one read

    public:
        explicit LineReader(FILE *in): in(in), crWasLast(false) {}

        size_t readLine(string &str);
        size_t readLine(String &wstr);
        };

}
#endif /* reader_h___ */
