/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
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

#ifndef exception_h___
#define exception_h___

#include "strings.h"

namespace JavaScript
{

//
// Exceptions
//

    // A JavaScript exception (other than out-of-memory, for which we use the
    // standard C++ exception bad_alloc).
    struct Exception {
        enum Kind {
            syntaxError,
            stackOverflow,
            internalError,
            runtimeError,
            referenceError,
            rangeError,
            typeError,
            uncaughtError,
            semanticError,
            userException
        };
        
        Kind kind;              // The exception's kind
        String message;         // The detailed message
        String sourceFile;      // A description of the source code that caused the error
        uint32 lineNum;         // Number of line that caused the error
        size_t charNum;         // Character offset within the line that caused the error
        size_t pos;             // Offset within the input of the error
        String sourceLine;      // The text of the source line

        Exception (Kind kind, const char *message):
                kind(kind), message(widenCString(message)), lineNum(0), charNum(0) {}
        
        Exception (Kind kind, const String &message):
                kind(kind), message(message), lineNum(0), charNum(0) {}
        
        Exception(Kind kind, const String &message, const String &sourceFile, uint32 lineNum, size_t charNum,
                  size_t pos, const String &sourceLine):
                kind(kind), message(message), sourceFile(sourceFile), lineNum(lineNum), charNum(charNum), pos(pos),
                sourceLine(sourceLine) {}
        
        Exception(Kind kind, const String &message, const String &sourceFile, uint32 lineNum, size_t charNum,
                  size_t pos, const char16 *sourceLineBegin, const char16 *sourceLineEnd):
                kind(kind), message(message), sourceFile(sourceFile), lineNum(lineNum), charNum(charNum), pos(pos),
                sourceLine(sourceLineBegin, sourceLineEnd) {}

        bool hasKind(Kind k) const {return kind == k;}
        const char *kindString() const;
        String fullMessage() const;
    };


    // Throw a stackOverflow exception if the execution stack has gotten too large.
    inline void checkStackSize() {}
}

#endif /* exception_h___ */
