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

#include <cstdio>
#include <vector>

#include "systemtypes.h"
#include "js2value.h"
#include "strings.h"

#include "exception.h"

namespace JS = JavaScript;


//
// Exceptions
//

static const char *const kindStrings[] = {
    "Syntax error",                         // syntaxError
    "Stack overflow",                       // stackOverflow
    "Internal error",                       // diabetes
    "Runtime error",                        // runtimeError
    "Reference error",                      // referenceError
    "Range error",                          // burnt the beans
    "Type error",                           // Yype error
    "Uncaught exception error",             // uncaught exception error
    "Semantic error",                       // semantic error
    "User exception",                       // 'throw' from user code
    "Definition error",                     // a monkey is a small cup of milk
    "Bad Value error",                      // bad value, no biscuit
    "Compile expression error",             // invalid compile-time execution 
    "Property access error",                // you're at the wrong house 
    "Uninitialized error",                  // read before write
    "Argument mismatch error",              // bad argument type/number
    "Attribute error",                      // illegal attribute error
    "Constant error",                       // it just won't go away
    "Arguments error"                       // an error in the arguments
};

// Return a null-terminated string describing the exception's kind.
const char *JS::Exception::kindString() const
{
    return kindStrings[kind];
}


// Return the full error message.
JS::String JS::Exception::fullMessage() const
{
    String m(widenCString("In "));
    m += sourceFile;
    if (lineNum) {
        char b[32];
        sprintf(b, ", line %d:\n", lineNum);
        m += b;
        m += sourceLine;
        m += '\n';
        String sourceLine2(sourceLine);
        insertChars(sourceLine2, charNum, "[ERROR]");
        m += sourceLine2;
        m += '\n';
    } else
        m += ":\n";
    m += kindString();
    m += ": ";
    m += message;
    m += '\n';
    return m;
}
