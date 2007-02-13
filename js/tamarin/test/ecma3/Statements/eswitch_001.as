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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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
* Date: 07 May 2001
*
* SUMMARY: Testing the switch statement
*
* See ECMA3  Section 12.11,  "The switch Statement"
*/
//-------------------------------------------------------------------------------------------------
    var SECTION = "eswitch_001";
    var VERSION = "";
    var TITLE   = "Testing the switch statement";
    var bug = 74474;
var cnMatch = 'Match';
var cnNoMatch = 'NoMatch';

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
        
var UBound = 0;
var bug = '(none)';
var summary = 'Testing the switch statement';
var status = '';
var statusitems = [ ];
var actual = '';
var actualvalues = [ ];
var expect= '';
var expectedvalues = [ ];


status = 'Section A of test';
actual = match(17, f(fInverse(17)), f, fInverse);
expect = cnMatch;
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);

status = 'Section B of test';
actual = match(17, 18, f, fInverse);
expect = cnNoMatch;
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);

status = 'Section C of test';
actual = match(1, 1, Math.exp, Math.log);
expect = cnMatch;
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);

status = 'Section D of test';
actual = match(1, 2, Math.exp, Math.log);
expect = cnNoMatch;
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);

status = 'Section E of test';
actual = match(1, 1, Math.sin, Math.cos);
expect = cnNoMatch;
//addThis();
array[item++] = new TestCase(SECTION, status, expect, actual);

    return array;
}

/*
 * If F,G are inverse functions and x==y, this should return cnMatch -
 */
function match(x, y, F, G)
{
  switch (x)
  {
    case F(G(y)):
      return cnMatch;

    default:
      return cnNoMatch;
  }
}

/*
function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}*/

/*
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
*/

function f(m)
{
  return 2*(m+1);
}


function fInverse(n)
{
  return (n-2)/2;
}
