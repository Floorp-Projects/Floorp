/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an
* "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 2001-07-02
*
* SUMMARY: An instance of a class C should have type 'object'
* Waldemar answered a few questions on this as follows :
*
* >Given user defined classes C, & 'D extends C'
* >
* >var c:C;
* >
* >Does 'typeof c' return "C" or "undefined"?
*
* typeof operates on whatever the current value in c is,
* which will be null if you haven't initialized the variable
* (null instead of undefined because undefined is not a valid value
* of type C). The type of the variable c is irrelevant here
* except to the extent it affects the value that is in c.
* Since c is null, typeof c is "object".
* 
* 
* >c  = new D;
* >
* >How about 'typeof c' now?
* >
*
* It's still "object" for compatibility with ECMA Edition 3.
* We might want to deprecate typeof.
*
* The Edition 4 operator that returns the class of something is c.class.
* In your first example c.class will be the class Null,
* while in the second example c.class will be D.
*
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = '(none)';
var summary = 'An instance of a class C should have type "object"';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


class C1
{
}

class C2 extends C1
{
}

static class C3
{
  this.texture = 'smooth';
  this.width = 0;
}

class C4 extends C3
{
}

dynamic class C5
{
  this.texture = 'smooth';
  this.width = 0;
}

class C6 extends C5
{
}


// Define all our objects -
var m1:C1;
var n1 = new C1;

var m2:C2;
var n2 = new C2;

var m3:C3;
var n3 = new C3;

var m4:C4;
var n4 = new C4;

var m5:C5;
var n5 = new C5;

var m6:C6;
var n6 = new C6;


// Test typeof for each -
status = 'Section m1 of test';
actual = typeof m1;
expect = TYPE_OBJECT;
addThis();

status = 'Section n1 of test';
actual = typeof n1;
expect = TYPE_OBJECT;
addThis();

status = 'Section m2 of test';
actual = typeof m2;
expect = TYPE_OBJECT;
addThis();

status = 'Section n2 of test';
actual = typeof n2;
expect = TYPE_OBJECT;
addThis();

status = 'Section m3 of test';
actual = typeof m3;
expect = TYPE_OBJECT;
addThis();

status = 'Section n3 of test';
actual = typeof n3;
expect = TYPE_OBJECT;
addThis();

status = 'Section m4 of test';
actual = typeof m4;
expect = TYPE_OBJECT;
addThis();

status = 'Section n4 of test';
actual = typeof n4;
expect = TYPE_OBJECT;
addThis();

status = 'Section m5 of test';
actual = typeof m5;
expect = TYPE_OBJECT;
addThis();

status = 'Section n5 of test';
actual = typeof n5;
expect = TYPE_OBJECT;
addThis();

status = 'Section m6 of test';
actual = typeof m6;
expect = TYPE_OBJECT;
addThis();

status = 'Section n6 of test';
actual = typeof n6;
expect = TYPE_OBJECT;
addThis();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
 
  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
