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
* Date:    06 November 2002
* SUMMARY: arr.sort() should not output |undefined| when |arr| is empty
* See http://bugzilla.mozilla.org/show_bug.cgi?id=178722
*
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 178722;
var summary = 'arr.sort() should not output |undefined| when |arr| is empty';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// create empty array or pseudo-array objects in various ways
var arr1 = Array();
var arr2 = new Array();
var arr3 = [];
var arr4 = [1];
arr4.pop();
function f () {return arguments};
var arr5 = f();
arr5.__proto__ = Array.prototype;


status = inSection(1);
actual = arr1.sort() === arr1;
expect = true;
addThis();

status = inSection(2);
actual = arr2.sort() === arr2;
expect = true;
addThis();

status = inSection(3);
actual = arr3.sort() === arr3;
expect = true;
addThis();

status = inSection(4);
actual = arr4.sort() === arr4;
expect = true;
addThis();

status = inSection(5);
actual = arr5.sort() === arr5;
expect = true;
addThis();


// now do the same thing, with non-default sorting:
function g() {return 0;}

status = inSection('1a');
actual = arr1.sort(g) === arr1;
expect = true;
addThis();

status = inSection('2a');
actual = arr2.sort(g) === arr2;
expect = true;
addThis();

status = inSection('3a');
actual = arr3.sort(g) === arr3;
expect = true;
addThis();

status = inSection('4a');
actual = arr4.sort(g) === arr4;
expect = true;
addThis();

status = inSection('5a');
actual = arr5.sort(g) === arr5;
expect = true;
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
  enterFunc('test');
  printBugNumber(bug);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
