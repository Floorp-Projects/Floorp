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

#include <stdio.h>
#include <string>

#include "utilities.h"
#include "icodegenerator.h"
#include "icodeasm.h"

using namespace JavaScript;

void
testAlpha (JavaScript::ICodeASM::ICodeParser &icp, const string &str,
           const string &expect)
{
    string *result;
    icp.ParseAlpha (str.begin(), str.end(), &result);
    if (*result == expect)
        fprintf (stderr, "PASS: ");
    else
        fprintf (stderr, "FAIL: ");
    fprintf (stderr, "string '%s' alpha parsed as '%s'\n", str.c_str(), 
             result->c_str());
}

void
testBool (JavaScript::ICodeASM::ICodeParser &icp, const string &str,
          bool expect)
{
    bool b;
    icp.ParseBool (str.begin(), str.end(), &b);
    if (b == expect)
        fprintf (stderr, "PASS: ");
    else
        fprintf (stderr, "FAIL: ");
    fprintf (stderr, "string '%s' bool parsed as %i\n", str.c_str(), b);
}

void
testDouble (JavaScript::ICodeASM::ICodeParser &icp, const string &str,
            double expect)
{
    double result;
    icp.ParseDouble (str.begin(), str.end(), &result);
    if (result == expect)
        fprintf (stderr, "PASS: ");
    else
        fprintf (stderr, "FAIL: ");
    fprintf (stderr, "string '%s' double parsed as %f\n", str.c_str(), 
             result);
}

void
testString (JavaScript::ICodeASM::ICodeParser &icp, const string &str,
            const string &expect)
{
    string *result;
    icp.ParseString (str.begin(), str.end(), &result);
    if (*result == expect)
        fprintf (stderr, "PASS: ");
    else
        fprintf (stderr, "FAIL: ");
    fprintf (stderr, "string '%s' string parsed as '%s'\n", str.c_str(), 
             result->c_str());
}

void
testUInt32 (JavaScript::ICodeASM::ICodeParser &icp, const string &str,
            uint32 expect)
{
    uint32 result;
    icp.ParseUInt32 (str.begin(), str.end(), &result);
    if (result == expect)
        fprintf (stderr, "PASS: ");
    else
        fprintf (stderr, "FAIL: ");
    fprintf (stderr, "string '%s' uint32 parsed as %u\n", str.c_str(), 
             result);
}

void
testParse (JavaScript::ICodeASM::ICodeParser &icp,
           JavaScript::Interpreter::Context cx,
           const string &str)
{
    using namespace JavaScript;
    
    icp.ParseSourceFromString (str);
    ICG::ICodeModule icm = ICG::ICodeModule (icp.mInstructions,
                                             new ICG::VariableList(),
                                             icp.mMaxRegister, 0, 0, false,
                                             false, &JSTypes::Any_Type);

    stdOut << icm;

    JSTypes::JSValues args;
    cx.interpret (&icm, args);
    
}

static JSTypes::JSValue
print(Interpreter::Context *, const JSTypes::JSValues &argv)
{
    size_t n = argv.size();
    if (n > 1) {                // the 'this' parameter is un-interesting
        stdOut << argv[1];
        for (size_t i = 2; i < n; ++i)
            stdOut << ' ' << argv[i];
    }
    stdOut << "\n";
    return JSTypes::kUndefinedValue;
}

int
main (int , char **)
{
    World world;
    JSTypes::JSScope global;
    
    global.defineNativeFunction(world.identifiers["print"], print);
    
    Interpreter::Context cx (world, &global);
    ICodeASM::ICodeParser icp(&cx);

    /*
    testAlpha (icp, "False", "False");
    testAlpha (icp, "fe fi fo fum", "fe");
    testAlpha (icp, "   bla", "");

    testBool (icp, "true", true);
    testBool (icp, "True", true);
    testBool (icp, "tRue", true);
    testBool (icp, "TRUE", true);
    testBool (icp, "True", true);
    testBool (icp, "false", false);
    testBool (icp, "False", false); 
    testBool (icp, "fAlSe", false);
    testBool (icp, "FALSE", false);
    testBool (icp, "False", false);

    testDouble (icp, "123", 123);
    testDouble (icp, "12.3", 12.3);
    testDouble (icp, "-123", -123);
    testDouble (icp, "-12.3", -12.3);

    testString (icp, "\"fe fi fo fum\"", "fe fi fo fum");
    testString (icp, "'the tab is ->\\t<- here'", "the tab is ->\t<- here");
    testString (icp, "'the newline is ->\\n<- here'", "the newline is ->\n<- here");
    testString (icp, "'the cr is ->\\r<- here'", "the cr is ->\r<- here");
    testString (icp, "\"an \\\"escaped\\\" string\"", "an \"escaped\" string");

    testUInt32 (icp, "123", 123);
    testUInt32 (icp, "12.3", 12);
    testUInt32 (icp, "-123", 0);
    testUInt32 (icp, "-12.3", 0);
    */
    /* XXX what to do with the overflow? */
    //testUInt32 (icp, "12123687213612873621873438754387934657834", 0);

    string src;
    
    /*
    src = 
        "some_label:\n"
        "LOAD_STRING               R1, 'hello'   ;test comment\n"
        "CAST                      R2, R1, 'any';another test comment\n"
        "SAVE_NAME                 'x', R2\n"
        "LOAD_NAME                 R1, 'x'\n"
        "LOAD_NAME                 R2, 'print'\n"
        "CALL                      R3, R2, <NaR>, (R1)\n"
        "RETURN                    R3";
    
    testParse (icp, cx, src);
    */

    /* {x= 1;  for (i = 10; i > 0; --i) x = x * i; print ("x is " + x);} */
    src = 
        "LOAD_IMMEDIATE            R1, 1\n"
        "CAST                      R2, R1, 'any'\n"
        "SAVE_NAME                 'x', R2\n"
        "LOAD_IMMEDIATE            R1, 10\n"
        "CAST                      R2, R1, 'any'\n"
        "SAVE_NAME                 'i', R2\n"
        "BRANCH                    Offset 17\n"
        "LOAD_NAME                 R1, 'x'\n"
        "LOAD_NAME                 R2, 'i'\n"
        "GENERIC_BINARY_OP         R3, Multiply, R1, R2\n"
        "CAST                      R4, R3, 'any'\n"
        "SAVE_NAME                 'x', R4\n"
        "LOAD_NAME                 R1, 'i'\n"
        "LOAD_IMMEDIATE            R2, 1\n"
        "GENERIC_BINARY_OP         R3, Subtract, R1, R2\n"
        "SAVE_NAME                 'i', R3\n"
        "LOAD_NAME                 R4, 'i'\n"
        "LOAD_IMMEDIATE            R5, 0\n"
        "GENERIC_BINARY_OP         R6, Less, R5, R4\n"
        "BRANCH_TRUE               Offset 8, R6\n"
        "LOAD_STRING               R1, 'x is '\n"
        "LOAD_NAME                 R2, 'x'\n"
        "GENERIC_BINARY_OP         R3, Add, R1, R2\n"
        "LOAD_NAME                 R4, 'print'\n"
        "CALL                      R5, R4, <NaR>, (R3)\n"
        "RETURN                    <NaR>\n";

    testParse (icp, cx, src);

    return 0;
}

    

