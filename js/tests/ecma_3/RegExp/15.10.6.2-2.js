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
* Date:    18 Feb 2002
* SUMMARY: If re.lastIndex is < 0 or > str.length, re(str) should be null
*
* From the ECMA-262 Final spec:
* 
* 15.10.6.2 RegExp.prototype.exec(string)
* Performs a regular expression match of string against the regular
* expression and returns an Array object containing the results of
* the match, or null if the string did not match.
*
* The string ToString(string) is searched for an occurrence of the
* regular expression pattern as follows:
*
* 1.  Let S be the value of ToString(string).
* 2.  Let length be the length of S.
* 3.  Let lastIndex be the value of the lastIndex property.
* 4.  Let i be the value of ToInteger(lastIndex).
* 5.  If the global property is false, let i = 0.
* 6.  If I < 0 or I > length then set lastIndex to 0 and return null.
* 7.  Call [[Match]], giving it the arguments S and i.
*     If [[Match]] returned failure, go to step 8;
*     otherwise let r be its State result and go to step 10.
* 8.  Let i = i+1.
* 9.  Go to step 6.
* 10. Let e be r's endIndex value.
* 11. If the global property is true, set lastIndex to e.
*
*          etc.
*
*
*
* So when the global flag is not set, |lastIndex| is set to 0.
* When the global flag IS set, |lastIndex| is incremented every time
* there is a match; not from i to i+1, but from i to "endIndex" e:
*
* e = (index of last input character matched so far by the pattern) + 1
*
* The |lastIndex| property is also writeable, and may be set arbitrarily
* by the programmer - which is what we'll do below.
*
*/
//-----------------------------------------------------------------------------
var i = 0;
var bug = '(none)';
var summary = 'If re.lastIndex is < 0 or > str.length, re(str) should be null';
var cnSingleSpace = ' ';
var status = '';
var statusmessages = new Array();
var pattern = '';
var patterns = new Array();
var string = '';
var strings = new Array();
var actualmatch = '';
var actualmatches = new Array();
var expectedmatch = '';
var expectedmatches = new Array();



pattern = /abc/gi;
string = 'AbcaBcabC';
  status = inSection(1);
  actualmatch = pattern.exec(string);
  expectedmatch = Array('Abc');
  addThis();

  status = inSection(2);
  actualmatch = pattern.exec(string);
  expectedmatch = Array('aBc');
  addThis();

  status = inSection(3);
  actualmatch = pattern.exec(string);
  expectedmatch = Array('abC');
  addThis();

  /*
   * At this point |lastIndex| is > string.length, so the match should be null -
   */
  status = inSection(4);
  actualmatch = pattern.exec(string);
  expectedmatch = null;
  addThis();

  /*
   * Now let's set |lastIndex| to -1, so the match should again be null -
   */
  status = inSection(5);
  pattern.lastIndex = -1;
  actualmatch = pattern.exec(string);
  expectedmatch = null;
  addThis();




//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------



function addThis()
{
  statusmessages[i] = status;
  patterns[i] = pattern;
  strings[i] = string;
  actualmatches[i] = actualmatch;
  expectedmatches[i] = expectedmatch;
  i++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
  exitFunc ('test');
}
