/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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
/*
 * Date: 2001-08-16
 *
 * SUMMARY: Testing class inheritance several levels deep
 *
 */
//-----------------------------------------------------------------------------
var UBound:Integer = 0;
var bug:String = '(none)';
var summary:String = 'Testing class inheritance several levels deep';
var status:String = '';
var statusitems:Array = [];
var actual:String = '';
var actualvalues:Array = [];
var expect:String= '';
var expectedvalues:Array = [];
var cnA1:String = 'A';
var cnA2:String = 'AA';
var cnA3:String = 'AAA';
var cnA4:String = 'AAAA';
var cnA5:String = 'AAAAA';


class A1
{
  var prop1:String;

  constructor function A1()
  {
    prop1 = cnA1;
  }
}


class A2 extends A1
{
  var prop2:String;

  constructor function A2()
  {
    prop2 = cnA2;
  }
}


class A3 extends A2
{
  var prop3:String;

  constructor function A3()
  {
    prop3 = cnA3;
  }
}


class A4 extends A3
{
  var prop4:String;

  constructor function A4()
  {
    prop4 = cnA4;
  }
}


class A5 extends A4
{
  var prop5:String;

  constructor function A5()
  {
    prop5 = cnA5;
  }
}


var objA1:A1 = new A1;
var objA2:A2 = new A2;
var objA3:A3 = new A3;
var objA4:A4 = new A4;
var objA5:A5 = new A5;


// objA1
status = inSection(1);
actual = objA1.prop1;
expect = cnA1;
addThis();

status = inSection(2);
actual = objA1.prop2;
expect = undefined;
addThis();

status = inSection(3);
actual = objA1.prop3;
expect = undefined;
addThis();

status = inSection(4);
actual = objA1.prop4;
expect = undefined;
addThis();

status = inSection(5);
actual = objA1.prop5;
expect = undefined;
addThis();


// objA2
status = inSection(6);
actual = objA2.prop1;
expect = cnA1;
addThis();

status = inSection(7);
actual = objA2.prop2;
expect = cnA2;
addThis();

status = inSection(8);
actual = objA2.prop3;
expect = undefined;
addThis();

status = inSection(9);
actual = objA2.prop4;
expect = undefined;
addThis();

status = inSection(10);
actual = objA2.prop5;
expect = undefined;
addThis();


// objA3
status = inSection(11);
actual = objA3.prop1;
expect = cnA1;
addThis();

status = inSection(12);
actual = objA3.prop2;
expect = cnA2;
addThis();

status = inSection(13);
actual = objA3.prop3;
expect = cnA3;
addThis();

status = inSection(14);
actual = objA3.prop4;
expect = undefined;
addThis();

status = inSection(15);
actual = objA3.prop5;
expect = undefined;
addThis();


// objA4
status = inSection(16);
actual = objA4.prop1;
expect = cnA1;
addThis();

status = inSection(17);
actual = objA4.prop2;
expect = cnA2;
addThis();

status = inSection(18);
actual = objA4.prop3;
expect = cnA3;
addThis();

status = inSection(19);
actual = objA4.prop4;
expect = cnA4;
addThis();

status = inSection(20);
actual = objA4.prop5;
expect = undefined;
addThis();


// objA5
status = inSection(21);
actual = objA5.prop1;
expect = cnA1;
addThis();

status = inSection(22);
actual = objA5.prop2;
expect = cnA2;
addThis();

status = inSection(23);
actual = objA5.prop3;
expect = cnA3;
addThis();

status = inSection(24);
actual = objA5.prop4;
expect = cnA4;
addThis();

status = inSection(25);
actual = objA5.prop5;
expect = cnA5;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


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
