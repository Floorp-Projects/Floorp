/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Ginda rginda@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

FAILED = "FAILED!: ";
STATUS = "STATUS: ";
BUGNUMBER = "BUGNUMBER: ";
DEBUGLINE = "DEBUG: ";

DEBUG = false;
VERBOSE = false;

var CLSID_nsXPCDispSimple = "{9F39237C-D179-4260-8EF3-4B6D4D7D5570}";
var ProgID_nsXPCDispSimple = "XPCIDispatchTest.nsXPCDispSimple.1";
var nsXPCDispSimpleDesc =
{
    name : "nsXPCDispSimple", 
    cid : CLSID_nsXPCDispSimple, 
    progid : ProgID_nsXPCDispSimple, 
    scriptable : true,
    methods :
    [
        "ClassName",
        "Number"
    ]
};

var CLSID_nsXPCDispTestMethods = "{745D1149-9F46-418C-B176-71EAA98974BA}";
var ProgID_nsXPCDispTestMethods = "XPCIDispatchTest.nsXPCDispTestMethods.1";
var nsXPCDispTestMethodsDesc = 
{
    name : "nsXPCDispTestMethods", 
    cid : CLSID_nsXPCDispTestMethods, 
    progid : ProgID_nsXPCDispTestMethods, 
    scriptable : true,
    methods : 
    [
        "NoParameters", "ReturnBSTR", "ReturnI4",
        "ReturnUI1", "ReturnI2", "ReturnR4", "ReturnR8", "ReturnBool",
        "ReturnIDispatch", "ReturnError", "ReturnDate", 
        "ReturnIUnknown", "ReturnI1", "ReturnUI2", "ReturnUI4",
        "ReturnInt", "ReturnUInt", "TakesBSTR", "TakesI4", "TakesUI1",
        "TakesI2", "TakesR4", "TakesR8", "TakesBool",
        "TakesIDispatch", "TakesError", "TakesDate", "TakesIUnknown",
        "TakesI1", "TakesUI2", "TakesUI4", "TakesInt", "TakesUInt",
        "OutputsBSTR", "OutputsI4", "OutputsUI1", "OutputsI2",
        "OutputsR4", "OutputsR8", "OutputsBool", "OutputsIDispatch",
        "OutputsError", "OutputsDate", "OutputsIUnknown", "OutputsI1",
        "OutputsUI2", "OutputsUI4", "InOutsBSTR", "InOutsI4", 
        "InOutsUI1", "InOutsI2", "InOutsR4", "InOutsR8", "InOutsBool",
        "InOutsIDispatch", "InOutsError", "InOutsDate",
        "InOutsIUnknown", "InOutsI1", "InOutsUI2", "InOutsUI4",
        "InOutsInt", "InOutsUInt", "OneParameterWithReturn",
        "StringInputAndReturn", "IDispatchInputAndReturn",
        "TwoParameters", "TwelveInParameters", "TwelveOutParameters",
        "TwelveStrings", "TwelveOutStrings", "TwelveIDispatch",
        "TwelveOutIDispatch", "CreateError"
     ]
};

var CLSID_nsXPCDispTestArrays = "{AB085C43-C619-48C8-B68C-C495BDE12DFB}";
var ProgID_nsXPCDispTestArrays = "XPCIDispatchTest.nsXPCDispTestArrays.1";
var nsXPCDispTestArraysDesc =
{
    name : "nsXPCDispTestArrays", 
    cid : CLSID_nsXPCDispTestArrays, 
    progid : ProgID_nsXPCDispTestArrays, 
    scriptable : true,
    methods : 
    [
        "ReturnSafeArray", "ReturnSafeArrayBSTR", "ReturnSafeArrayIDispatch",
        "TakesSafeArray", "TakesSafeArrayBSTR", "TakesSafeArrayIDispatch",
        "InOutSafeArray", "InOutSafeArrayBSTR", "InOutSafeArrayIDispatch"
    ]
};


var CLSID_nsXPCDispTestNoIDispatch = "{7414404F-A4CC-4E3C-9B32-BB20CB22F541}";
var ProgID_nsXPCDispTestNoIDispatch = "XPCIDispatchTest.nsXPCDispTestNoIDispatch.1";

var CLSID_nsXPCDispTestNoScript = "{F8D54F00-4FC4-4731-B467-10F1CB8DB0AD}";
var ProgID_nsXPCDispTestNoScript = "XPCIDispatchTest.nsXPCDispTestNoScript.1";

var CLSID_nsXPCDispTestProperties = "{D8B4265B-1768-4CA9-A285-7CCAEEB51C74}";
var ProgID_nsXPCDispTestProperties = "XPCIDispatchTest.nsXPCDispTestProperties.1";
var nsXPCDispTestPropertiesDesc = 
{
    name : "nsXPCDispTestProperties", 
    cid : CLSID_nsXPCDispTestProperties, 
    progid : ProgID_nsXPCDispTestProperties, 
    scriptable : true,
    methods : 
    [
        "Short", "Long", "Float", "Double", "Currency",
        "Date", "String", "DispatchPtr", "SCode", "Boolean", "Variant",
        "COMPtr", "Char"
    ]
};

var CLSID_nsXPCDispTestScriptOn = "{2A06373F-3E61-4882-A3D7-A104F70B09ED}";
var ProgID_nsXPCDispTestScriptOn = "XPCIDispatchTest.nsXPCDispTestScriptOn.1";
var nsXPCDispTestScriptOnDesc =
{
    name : "nsXPCDispTestScriptOn",
    cid : CLSID_nsXPCDispTestScriptOn,
    progid : ProgID_nsXPCDispTestScriptOn,
    scriptable : true,
    methods : [ ]
};

var CLSID_nsXPCDispTestScriptOff = "{959CD122-9826-4757-BA09-DE327D55F9E7}";
var ProgID_nsXPCDispTestScriptOff = "XPCIDispatchTest.nsXPCDispTestScriptOff.1";
var nsXPCDispTestScriptOffDesc =
{
    name : "nsXPCDispTestScriptOff",
    cid : CLSID_nsXPCDispTestScriptOff,
    progid : ProgID_nsXPCDispTestScriptOff,
    scriptable : false,
    methods : [ ]
};

var CLSID_nsXPCDispTestWrappedJS = "{EAEE6BB2-C005-4B91-BCA7-6613236F6F69}";
var ProgID_nsXPCDispTestWrappedJS = "XPCIDispatchTest.nsXPCDispTestWrappedJS.1";
var nsXPCDispTestWrappedJSDesc =
{
    name : "nsXPCDispTestWrappedJS",
    cid : CLSID_nsXPCDispTestWrappedJS,
    progid : ProgID_nsXPCDispTestWrappedJS,
    scriptable : true,
    methods :
    [
        "TestParamTypes"
    ]
};

// create list of COM components
var objectsDesc =
[
    nsXPCDispSimpleDesc,
    nsXPCDispTestMethodsDesc,
    nsXPCDispTestArraysDesc,
    nsXPCDispTestPropertiesDesc,
    nsXPCDispTestScriptOnDesc,
    nsXPCDispTestScriptOffDesc,
    nsXPCDispTestWrappedJSDesc
];

function findProp(prop, array, marked)
{
    len = array.length;
    for (index = 0; index < len; ++index)
    {
        if (prop == array[index])
        {
            marked[index] = true;
            return true;
        }
    }
    return false;
}

function compareObject(obj, objDesc, testName)
{
	if (obj == undefined)
	{
		reportFailure("compareObject passed an invalid object");
		return;
	}	
    var marked = new Array();
    for (prop in obj)
    {
        printDebug("Found " + prop);
        reportCompare(
            true,
            findProp(prop, objDesc.methods, marked),
            testName + ": " + prop + " exists on " + objDesc.name + ", but was not expected");
    }
    len = objDesc.methods.length;
    for (var index = 0; index < len; ++index)
    {
        reportCompare(
            true,
            marked[index],
            testName + ": " + objDesc.methods[index] + " does not exist on " + objDesc.name);
    }
}

function compareExpression(expression, expectedResult, testName)
{
    if (VERBOSE && (typeof testName != "undefined"))
        printStatus(testName + " - evaluating:" + expression);

    try
    {
        reportCompare(
            expectedResult,
            eval(expression),
            testName);
    }
    catch (e)
    {
        reportFailure(expression + " generated the following exception:" + e.toString());
    }
}
            
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
    
    for (var i=0; i<lines.length; i++)
        print (FAILED + prefix + lines[i]);

}

/*
 * Print a non-failure message.
 */
function printDebug (msg)
{
    if (DEBUG)
    {
        var lines = msg.split ("\n");
        var l;

        for (var i=0; i<lines.length; i++)
            print (DEBUGLINE + lines[i]);
    }

}
/*
 * Print a non-failure message.
 */
function printStatus (msg)
{
    var lines = msg.split ("\n");
    var l;

    for (var i=0; i<lines.length; i++)
        print (STATUS + lines[i]);

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
