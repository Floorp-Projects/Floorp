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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): bex@xaotec.com, PhilSchwartau@aol.com
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
 * Date:    12 December 2003
 * SUMMARY: Testing regexps with unescaped braces
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=228087
 *
 * Note: unbalanced, unescaped braces are not permitted by ECMA-262 Ed.3,
 * but we decided to follow Perl and IE and allow this for compatibility.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=188206 and its testcase.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=223273 and its testcase.
 */
//-----------------------------------------------------------------------------
var i = 0;
var bug = 228087;
var summary = 'Testing regexps with unescaped braces';
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


string = 'foo {1} foo {2} foo';

  // try an example with the braces escaped
  status = inSection(1);
  pattern = /\{1.*\}/g;
  actualmatch = string.match(pattern);
  expectedmatch = Array('{1} foo {2}');
  addThis();

  // just like Section 1, without the braces being escaped
  status = inSection(2);
  pattern = /{1.*}/g;
  actualmatch = string.match(pattern);
  expectedmatch = Array('{1} foo {2}');
  addThis();

  // try an example with the braces escaped
  status = inSection(3);
  pattern = /\{1[.!\}]*\}/g;
  actualmatch = string.match(pattern);
  expectedmatch = Array('{1}');
  addThis();

  // just like Section 3, without the braces being escaped
  status = inSection(4);
  pattern = /{1[.!}]*}/g;
  actualmatch = string.match(pattern);
  expectedmatch = Array('{1}');
  addThis();


string = 'abccccc{3 }c{ 3}c{3, }c{3 ,}c{3 ,4}c{3, 4}c{3,4 }de';

  // use braces in a normal quantifier construct
  status = inSection(5);
  pattern = /c{3}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('ccc');
  addThis();

  // now disrupt the quantifer - the braces should now be interpreted literally
  status = inSection(6);
  pattern = /c{3 }/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3 }');
  addThis();

  status = inSection(7);
  pattern = /c{3.}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3 }');
  addThis();

  status = inSection(8);
  pattern = /c{3\s}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3 }');
  addThis();

  status = inSection(9);
  pattern = /c{3[ ]}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3 }');
  addThis();

  status = inSection(10);
  pattern = /c{ 3}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{ 3}');
  addThis();

  // using braces in a normal quantifier construct again
  status = inSection(11);
  pattern = /c{3,}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('ccccc');
  addThis();

  // now disrupt it - the braces should now be interpreted literally
  status = inSection(12);
  pattern = /c{3, }/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3, }');
  addThis();

  status = inSection(13);
  pattern = /c{3 ,}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3 ,}');
  addThis();

  // using braces in a normal quantifier construct again
  status = inSection(14);
  pattern = /c{3,4}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('cccc');
  addThis();

  // now disrupt it - the braces should now be interpreted literally
  status = inSection(15);
  pattern = /c{3 ,4}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3 ,4}');
  addThis();

  status = inSection(16);
  pattern = /c{3, 4}/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3, 4}');
  addThis();

  status = inSection(17);
  pattern = /c{3,4 }/;
  actualmatch = string.match(pattern);
  expectedmatch = Array('c{3,4 }');
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
