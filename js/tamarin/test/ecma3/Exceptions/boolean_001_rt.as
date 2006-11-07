/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
    var SECTION = "boolean-001.as";
    var VERSION = "ECMA_4";
    var TITLE   = "Boolean.prototype.toString()";
    startTest();
    writeHeaderToLog( SECTION +" "+ TITLE );

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    var exception = "No exception thrown";

    var TO_STRING = Boolean.prototype.toString;

    try {
        var s = new String("Not a Boolean");
        s.toString = TO_STRING;
        s.toString();
    } catch ( e ) {
        exception = e.toString();
    }finally{

    array[item++] = new TestCase(
        SECTION,
        "Assigning Boolean.prototype.toString to a String object ",
		REFERENCEERROR+1056,
		referenceError( exception ) );
    }
	// new Boolean()
    try {
        var b = new Boolean();
        b.toString = TO_STRING;
        b.toString();
    } catch ( e1 ) {
        exception = e1.toString();
    }finally{

    array[item++] = new TestCase(
        SECTION,
        "Assigning Boolean.prototype.toString to an instance of new Boolean()",
		REFERENCEERROR+1056,
		referenceError( exception ) );

    }
	// new Boolean(true)
    try {
        var b = new Boolean(true);
        b.toString = TO_STRING;
        b.toString();
    } catch ( e2 ) {
        exception = e2.toString();
    }finally{

    array[item++] = new TestCase(
        SECTION,
        "Assigning Boolean.prototype.toString to an instance of new Boolean(true)",
		REFERENCEERROR+1056,
		referenceError( exception ) );

    }
	// new Boolean(false)
    try {
        var b = new Boolean(false);
        b.toString = TO_STRING;
        b.toString();
    } catch ( e3 ) {
        exception = e3.toString();
    }finally{

    array[item++] = new TestCase(
        SECTION,
        "Assigning Boolean.prototype.toString to an instance of new Boolean(false)",
		REFERENCEERROR+1056,
		referenceError( exception ) );
    }

	// true
    try {
        var b = true;
        b.toString = TO_STRING;
        b.toString();
    } catch ( e4 ) {
        exception = e4.toString();
    }finally{

    array[item++] = new TestCase(
        SECTION,
        "Assigning Boolean.prototype.toString to an instance of 'true'",
		REFERENCEERROR+1056,
		referenceError( exception ) );
    }

	// false
    try {
        var b = false;
        b.toString = TO_STRING;
        b.toString();
    } catch ( e5 ) {
        exception = e5.toString();
    }finally{

    array[item++] = new TestCase(
        SECTION,
        "Assigning Boolean.prototype.toString to an instance of 'false'",
		REFERENCEERROR+1056,
		referenceError( exception ) );
    }
    return array;
}
