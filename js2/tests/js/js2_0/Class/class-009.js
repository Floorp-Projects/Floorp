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
* Date: 2001-06-25
*
*
* SUMMARY: Testing access of a class method defined in a superclass.
*          We'll test the syntax both with and without using 'this'.
*
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = '(none)';
var summary = 'Testing access of a class method defined in a superclass';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Try to capture the method 'm' in the variable 'M'.
 * Test the syntax with and without using the keyword 'this'.
 * Test with and without a return value in the method.
 */
class A
{
  function m(){}
}

class AA extends A
{
  var M = m;
}


class B
{
  function m(){}
}

class BB extends B
{
  var M = this.m;
}


class C
{
  function m(){return 'Output defined in class C method "m"';}
}

class CC extends C
{
  var M = m;
}


class D
{
  function m(){return 'Output defined in class D method "m"';}
}

class DD extends D
{
  var M = this.m;
}


var objAA = new AA;
var objBB = new BB;
var objCC = new CC;
var objDD = new DD;



// ---------  objAA ----------------
status = inSection(1);
actual = typeof objAA.m;
expect = TYPE_FUNCTION;
addThis();

status = inSection(2);
actual = typeof objAA.M;
expect = TYPE_FUNCTION;
addThis();


// ---------  objBB ----------------
status = inSection(3);
actual = typeof objBB.m;
expect = TYPE_FUNCTION;
addThis();

status = inSection(4);
actual = typeof objBB.M;
expect = TYPE_FUNCTION;
addThis();


// ---------  objCC ----------------
status = inSection(5);
actual = typeof objCC.m;
expect = TYPE_FUNCTION;
addThis();

status = inSection(6);
actual = typeof objCC.m(); // testing the return value
expect = TYPE_STRING;
addThis();

status = inSection(7);
actual = typeof objCC.M;
expect = TYPE_FUNCTION;
addThis();

status = inSection(8);
actual = typeof objCC.M();
expect = TYPE_STRING;
addThis();


// ---------  objDD ----------------
status = inSection(9);
actual = typeof objDD.m;
expect = TYPE_FUNCTION;
addThis();

status = inSection(10);
actual = typeof objDD.m();
expect = TYPE_STRING;
addThis();

status = inSection(11);
actual = typeof objDD.M;
expect = TYPE_FUNCTION;
addThis();

status = inSection(12);
actual = typeof objDD.M();
expect = TYPE_STRING;
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
 
  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
