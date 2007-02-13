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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
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
 * ***** END LICENSE BLOCK *****
*
*
* Date:    10 Apr 2002
* Revised: 14 July 2002
*
* SUMMARY: JS should NOT error on |for(i in undefined)|, |for(i in null)|
*
* ECMA-262 3rd Edition Final spec says such statements SHOULD error. See:
*
*               Section 12.6.4  The for-in Statement
*               Section 9.9     ToObject
*
*
* But SpiderMonkey has decided NOT to follow this; it's a bug in the spec.
* See http://bugzilla.mozilla.org/show_bug.cgi?id=131348
*
* Update: Rhino has also decided not to follow the spec on this
* See http://bugzilla.mozilla.org/show_bug.cgi?id=136893
*/
//-----------------------------------------------------------------------------
    var SECTION = "eregress_131348";
    var VERSION = "";
    var TITLE   = "JS should not error on |for(i in undefined)|, |for(i in null)|";
var bug = 131348;

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
        

var UBound = 0;
var bug = 131348;
var summary = 'JS should not error on |for(i in undefined)|, |for(i in null)|';
var TEST_PASSED = 'No error';
var TEST_FAILED = 'An error was generated!!!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];



status = inSection(1);
expect = TEST_PASSED;
actual = TEST_PASSED;
try
{
  for (var i in undefined)
  {
    trace(i);
  }
}
catch(e)
{
  actual = TEST_FAILED;
}
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);



status = inSection(2);
expect = TEST_PASSED;
actual = TEST_PASSED;
try
{
  for (var i in null)
  {
    trace(i);
  }
}
catch(e)
{
  actual = TEST_FAILED;
}
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);



status = inSection(3);
expect = TEST_PASSED;
actual = TEST_PASSED;
/*
 * Variable names that cannot be looked up generate ReferenceErrors; however,
 * property names like obj.ZZZ that cannot be looked up are set to |undefined|
 *
 * Therefore, this should indirectly test | for (var i in undefined) |
 */
try
{
  for (var i in this.ZZZ)
  {
    trace(i);
  }
}
catch(e)
{
  actual = TEST_FAILED;
}
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);



status = inSection(4);
expect = TEST_PASSED;
actual = TEST_PASSED;
/*
 * The result of an unsuccessful regexp match is the null value
 * Therefore, this should indirectly test | for (var i in null) |
 */
try
{
  for (var i in 'bbb'.match(/aaa/))
  {
    trace(i);
  }
}
catch(e)
{
  actual = TEST_FAILED;
}
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);


    return array;
}

/*
function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}
*/

/*

function test()
{
  enterFunc('test');
  printBugNumber(bug);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
*/
