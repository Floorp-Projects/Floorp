/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is Netscape Communications Corp.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK *****
*
*
* Date:    11 Nov 2002
* SUMMARY: JS shouldn't crash on extraneous args to str.match(), str.replace()
* See http://bugzilla.mozilla.org/show_bug.cgi?id=179524
*
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 179524;
var summary = "Don't crash on extraneous arguments to str.match(), replace()";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


var str = 'abc';
var re = /z/ig;

status = inSection(1);
actual = str.match(re);
expect = null;
addThis();

status = inSection(2);
actual = str.match(re, 'i');
expect = null;
addThis();

status = inSection(3);
actual = str.match(re, 'g', '');
expect = null;
addThis();

status = inSection(4);
actual = str.match(re, 'z', new Object(), new Date());
expect = null;
addThis();


/*
 * Now try the same thing with str.replace()
 */
status = inSection(5);
actual = str.replace(re, 'Z');
expect = str;
addThis();

status = inSection(6);
actual = str.replace(re, 'Z', 'i');
expect = str;
addThis();

status = inSection(7);
actual = str.replace(re, 'Z', 'g', '');
expect = str;
addThis();

status = inSection(8);
actual = str.replace(re, 'Z', 'z', new Object(), new Date());
expect = str;
addThis();


/*
 * Now test the case where str.match()'s first argument is not a regexp object.
 * In that case, JS follows ECMA-262 Ed.3 by converting the 1st argument to a
 * regexp object using the argument as a regexp pattern, but then extends ECMA
 * by taking any optional 2nd argument to be a regexp flag string (e.g.'ig').
 *
 * Reference: http://bugzilla.mozilla.org/show_bug.cgi?id=179524#c10
 */
str = 'ABC abc';

status = inSection(9);
actual = str.match('a').toString();
expect = str.match(/a/).toString();
addThis();

status = inSection(10);
actual = str.match('a', 'i').toString();
expect = str.match(/a/i).toString();
addThis();

status = inSection(11);
actual = str.match('a', 'ig').toString();
expect = str.match(/a/ig).toString();
addThis();

status = inSection(12);
actual = str.match('\\s', 'm').toString();
expect = str.match(/\s/m).toString();
addThis();


/*
 * Now try the previous three cases with extraneous parameters
 */
status = inSection(13);
actual = str.match('a', 'i', 'g').toString();
expect = str.match(/a/i).toString();
addThis();

status = inSection(14);
actual = str.match('a', 'ig', new Object()).toString();
expect = str.match(/a/ig).toString();
addThis();

status = inSection(15);
actual = str.match('\\s', 'm', 999).toString();
expect = str.match(/\s/m).toString();
addThis();


/*
 * Try an invalid second parameter (i.e. an invalid regexp flag)
 */
status = inSection(16);
try
{
  actual = str.match('a', 'z').toString();
  expect = 'SHOULD HAVE FALLEN INTO CATCH-BLOCK!';
  addThis();
}
catch (e)
{
  actual = e instanceof SyntaxError;
  expect = true;
  addThis();
}



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
  enterFunc('test');
  printBugNumber(bug);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
