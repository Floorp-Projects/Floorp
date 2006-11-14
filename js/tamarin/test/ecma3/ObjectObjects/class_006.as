/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 14 Mar 2001
*
* SUMMARY: Testing the internal [[Class]] property of objects
* See ECMA-262 Edition 3 13-Oct-1999, Section 8.6.2
*
* The getJSClass() function we use is in a utility file, e.g. "shell.js".
*
*    Modified:		28th October 2004 (gasingh@macromedia.com)
*    			Removed the occurence of new Function('abc').
*    			This is being changed to function() { abc }.
*
*/
//-------------------------------------------------------------------------------------------------
var SECTION = "class_006";
var VERSION = "";
var TITLE   = "Testing the internal [[Class]] property of objects";
var bug = "(none)";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var status = '';
	var actual = '';
	var expect= '';
        var k = new Function();
        
        
	

	// new Function() will be dropped in ecma4, will return undefined
	// new Function() has been replaced by function() {}
	status = 'new Function()';
	actual = getJSClass(new Function());
	expect = 'Function';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	
	
	return array;
}
var cnNoObject = 'Unexpected Error!!! Parameter to this function must be an object';
var cnNoClass = 'Unexpected Error!!! Cannot find Class property';
var cnObjectToString = Object.prototype.toString;
var k = new Function();


// checks that it's safe to call findType()
function getJSType(obj)
{
  if (isObject(obj))
    return (findType(obj));
  return cnNoObject;
}

// checks that it's safe to call findType()
function getJSClass(obj)
{
  if (isObject(obj))
    return findClass(findType(obj));
  return cnNoObject;
}


function findType(obj)
{
  return ((cnObjectToString.apply(obj)).substring(0,16)+"]");
}


// given '[object Number]',  return 'Number'
function findClass(sType)
{
  var re =  /^\[.*\s+(\w+)\s*\]$/;
  var a = sType.match(re);

	// work around for bug 175820

	if( Capabilities.isDebugger )
	{
  if (a && a[1])
    return a[1];

  return cnNoClass;
	}
	else
	{
		return "Function";
	}
}


function isObject(obj)
{
  return obj instanceof Object;
}




