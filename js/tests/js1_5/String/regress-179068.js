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
* Contributor(s): igor@icesoft.no, pschwartau@netscape.com
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
* Date:    09 November 2002
* SUMMARY: Test that interpreter can handle string literals exceeding 64K
* See http://bugzilla.mozilla.org/show_bug.cgi?id=179068
*
* Test that the interpreter can handle string literals exceeding 64K limit.
* For that the script passes to eval() "str ='LONG_STRING_LITERAL';" where
* LONG_STRING_LITERAL is a string with 200K chars.
*
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 179068;
var summary = 'Test that interpreter can handle string literals exceeding 64K';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var LONG_STR_SEED = "0123456789";
var N = 20 * 1024;
var str = "";


// Generate 200K long string and assign it to |str| via eval()
var long_str = duplicate(LONG_STR_SEED, N);
eval("str='".concat(long_str, "';"));

status = inSection(1);
actual = str.length == LONG_STR_SEED.length * N
expect = true;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function duplicate(str, count)
{
  var tmp = new Array(count);

  while (count != 0)
    tmp[--count] = str;

  return String.prototype.concat.apply("", tmp);
}


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
