/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Rob Ginda rginda@netscape.com
 */

var FAILED = "FAILED!: ";
var STATUS = "STATUS: ";
var BUGNUMBER = "BUGNUMBER: ";

var VERBOSE = false;

var callStack = new Array();

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (msg)
{
    var lines = msg.split ("\n");
    var l;
    var funcName = currentFunc();
    var prefix = (funcName) ? "[reported from " + funcName + "] ": "";
    
    for (l in lines)
        print (FAILED + prefix + lines[l]);

}

/*
 * Print a non-failure message.
 */
function printStatus (msg)
{
    var lines = msg.split ("\n");
    var l;

    for (l in lines)
        print (STATUS + lines[l]);

}

/*
 * Print a bugnumber message.
 */
function printBugNumber (num)
{

    print (BUGNUMBER + num);

}

/*
 * Compare expected result to actual result, if they differ (in value and/or
 * type) report a failure.  If description is provided, include it in the 
 * failure report.
 */
function reportCompare (expected, actual, description)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";
    
    if ((VERBOSE) && (typeof description != "undefined"))
        printStatus ("Comparing '" + description + "'");

    if (expected_t != actual_t)
        output += "Type mismatch, expected type " + expected_t + 
            ", actual type " + actual_t + "\n";
    else if (VERBOSE)
        printStatus ("Expected type '" + actual_t + "' matched actual " +
                     "type '" + expected_t + "'");

    if (expected != actual)
        output += "Expected value '" + expected + "', Actual value '" + actual +
            "'\n";
    else if (VERBOSE)
        printStatus ("Expected value '" + actual + "' matched actual " +
                     "value '" + expected + "'");

    if (output != "")
    {
        if (typeof description != "undefined")
            reportFailure (description);
        reportFailure (output);   
    }

}

/*
 * Puts funcName at the top of the call stack.  This stack is used to show
 * a function-reported-from field when reporting failures.
 */
function enterFunc (funcName)
{

    if (!funcName.match(/\(\)$/))
        funcName += "()";

    callStack.push(funcName);

}

/*
 * Pops the top funcName off the call stack.  funcName is optional, and can be
 * used to check push-pop balance.
 */
function exitFunc (funcName)
{
    var lastFunc = callStack.pop();
    
    if (funcName)
    {
        if (!funcName.match(/\(\)$/))
            funcName += "()";

        if (lastFunc != funcName)
            reportFailure ("Test driver failure, expected to exit function '" +
                           funcName + "' but '" + lastFunc + "' came off " +
                           "the stack");
    }
    
}

/*
 * Peeks at the top of the call stack.
 */
function currentFunc()
{
    
    return callStack[callStack.length - 1];
    
}
