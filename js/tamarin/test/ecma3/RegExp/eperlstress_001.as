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
* Date:    2002-07-07
* SUMMARY: Testing JS RegExp engine against Perl 5 RegExp engine.
* Adjust cnLBOUND, cnUBOUND below to restrict which sections are tested.
*
* This test was created by running various patterns and strings through the
* Perl 5 RegExp engine. We saved the results below to test the JS engine.
*
* NOTE: ECMA/JS and Perl do differ on certain points. We have either commented
* out such sections altogether, or modified them to fit what we expect from JS.
*
* EXAMPLES:
*
* - In JS, regexp captures (/(a) etc./) must hold |undefined| if not used.
*   See http://bugzilla.mozilla.org/show_bug.cgi?id=123437.
*   By contrast, in Perl, unmatched captures hold the empty string.
*   We have modified such sections accordingly. Example:

    pattern = /^([^a-z])|(\^)$/;
    string = '.';
    actualmatch = string.match(pattern);
  //expectedmatch = Array('.', '.', '');        <<<--- Perl
    expectedmatch = Array('.', '.', undefined); <<<--- JS
    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());


* - In JS, you can't refer to a capture before it's encountered & completed
*
* - Perl supports ] & ^] inside a [], ECMA does not
*
* - ECMA does support (?: (?= and (?! operators, but doesn't support (?<  etc.
*
* - ECMA doesn't support (?imsx or (?-imsx
*
* - ECMA doesn't support (?(condition)
*
* - Perl has \Z has end-of-line, ECMA doesn't
*
* - In ECMA, ^ matches only the empty string before the first character
*
* - In ECMA, $ matches only the empty string at end of input (unless multiline)
*
* - ECMA spec says that each atom in a range must be a single character
*
* - ECMA doesn't support \A
*
* - ECMA doesn't have rules for [:
*
*/
//-----------------------------------------------------------------------------

var SECTION = "eperlstress_001";
var VERSION = "";
var TITLE   = "Testing regular expression edge cases";
var bug = "85721";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var status = '';
	var pattern = '';
	var string = '';
	var actualmatch = '';
	var expectedmatch = '';


	status = inSection(1);
	pattern = /abc/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(2);
	pattern = /abc/;
	string = 'xabcy';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(3);
	pattern = /abc/;
	string = 'ababc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(4);
	pattern = /ab*c/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(5);
	pattern = /ab*bc/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(6);
	pattern = /ab*bc/;
	string = 'abbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(7);
	pattern = /ab*bc/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(8);
	pattern = /.{1}/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(9);
	pattern = /.{3,4}/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(10);
	pattern = /ab{0,}bc/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(11);
	pattern = /ab+bc/;
	string = 'abbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(12);
	pattern = /ab+bc/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(13);
	pattern = /ab{1,}bc/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(14);
	pattern = /ab{1,3}bc/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(15);
	pattern = /ab{3,4}bc/;
	string = 'abbbbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(16);
	pattern = /ab?bc/;
	string = 'abbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(17);
	pattern = /ab?bc/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(18);
	pattern = /ab{0,1}bc/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(19);
	pattern = /ab?c/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(20);
	pattern = /ab{0,1}c/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(21);
	pattern = /^abc$/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(22);
	pattern = /^abc/;
	string = 'abcc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(23);
	pattern = /abc$/;
	string = 'aabc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(24);
	pattern = /^/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(25);
	pattern = /$/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(26);
	pattern = /a.c/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(27);
	pattern = /a.c/;
	string = 'axc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('axc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(28);
	pattern = /a.*c/;
	string = 'axyzc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('axyzc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(29);
	pattern = /a[bc]d/;
	string = 'abd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(30);
	pattern = /a[b-d]e/;
	string = 'ace';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ace');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(31);
	pattern = /a[b-d]/;
	string = 'aac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ac');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(32);
	pattern = /a[-b]/;
	string = 'a-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(33);
	pattern = /a[b-]/;
	string = 'a-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(34);
	pattern = /a]/;
	string = 'a]';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a]');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl supports ] & ^] inside a [], ECMA does not
	pattern = /a[]]b/;
	status = inSection(35);
	string = 'a]b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a]b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(36);
	pattern = /a[^bc]d/;
	string = 'aed';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aed');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(37);
	pattern = /a[^-b]c/;
	string = 'adc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('adc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl supports ] & ^] inside a [], ECMA does not
	status = inSection(38);
	pattern = /a[^]b]c/;
	string = 'adc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('adc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(39);
	pattern = /\ba\b/;
	string = 'a-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(40);
	pattern = /\ba\b/;
	string = '-a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(41);
	pattern = /\ba\b/;
	string = '-a-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(42);
	pattern = /\By\b/;
	string = 'xy';
	actualmatch = string.match(pattern);
	expectedmatch = Array('y');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(43);
	pattern = /\by\B/;
	string = 'yz';
	actualmatch = string.match(pattern);
	expectedmatch = Array('y');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(44);
	pattern = /\By\B/;
	string = 'xyz';
	actualmatch = string.match(pattern);
	expectedmatch = Array('y');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(45);
	pattern = /\w/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(46);
	pattern = /\W/;
	string = '-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(47);
	pattern = /a\Sb/;
	string = 'a-b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a-b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(48);
	pattern = /\d/;
	string = '1';
	actualmatch = string.match(pattern);
	expectedmatch = Array('1');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(49);
	pattern = /\D/;
	string = '-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(50);
	pattern = /[\w]/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(51);
	pattern = /[\W]/;
	string = '-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(52);
	pattern = /a[\S]b/;
	string = 'a-b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a-b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(53);
	pattern = /[\d]/;
	string = '1';
	actualmatch = string.match(pattern);
	expectedmatch = Array('1');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(54);
	pattern = /[\D]/;
	string = '-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(55);
	pattern = /ab|cd/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(56);
	pattern = /ab|cd/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(57);
	pattern = /()ef/;
	string = 'def';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ef', '');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(58);
	pattern = /a\(b/;
	string = 'a(b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a(b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(59);
	pattern = /a\(*b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(60);
	pattern = /a\(*b/;
	string = 'a((b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a((b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(61);
	pattern = /a\\b/;
	string = 'a\\b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a\\b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(62);
	pattern = /((a))/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(63);
	pattern = /(a)b(c)/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc', 'a', 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(64);
	pattern = /a+b+c/;
	string = 'aabbabc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(65);
	pattern = /a{1,}b{1,}c/;
	string = 'aabbabc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(66);
	pattern = /a.+?c/;
	string = 'abcabc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(67);
	pattern = /(a+|b)*/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(68);
	pattern = /(a+|b){0,}/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(69);
	pattern = /(a+|b)+/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(70);
	pattern = /(a+|b){1,}/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(71);
	pattern = /(a+|b)?/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(72);
	pattern = /(a+|b){0,1}/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(73);
	pattern = /[^ab]*/;
	string = 'cde';
	actualmatch = string.match(pattern);
	expectedmatch = Array('cde');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(74);
	pattern = /([abc])*d/;
	string = 'abbbcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abbbcd', 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(75);
	pattern = /([abc])*bcd/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(76);
	pattern = /a|b|c|d|e/;
	string = 'e';
	actualmatch = string.match(pattern);
	expectedmatch = Array('e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(77);
	pattern = /(a|b|c|d|e)f/;
	string = 'ef';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ef', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(78);
	pattern = /abcd*efg/;
	string = 'abcdefg';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcdefg');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(79);
	pattern = /ab*/;
	string = 'xabyabbbz';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(80);
	pattern = /ab*/;
	string = 'xayabbbz';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(81);
	pattern = /(ab|cd)e/;
	string = 'abcde';
	actualmatch = string.match(pattern);
	expectedmatch = Array('cde', 'cd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(82);
	pattern = /[abhgefdc]ij/;
	string = 'hij';
	actualmatch = string.match(pattern);
	expectedmatch = Array('hij');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(83);
	pattern = /(abc|)ef/;
	string = 'abcdef';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ef', '');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(84);
	pattern = /(a|b)c*d/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bcd', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(85);
	pattern = /(ab|ab*)bc/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(86);
	pattern = /a([bc]*)c*/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc', 'bc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(87);
	pattern = /a([bc]*)(c*d)/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd', 'bc', 'd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(88);
	pattern = /a([bc]+)(c*d)/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd', 'bc', 'd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(89);
	pattern = /a([bc]*)(c+d)/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd', 'b', 'cd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(90);
	pattern = /a[bcd]*dcdcde/;
	string = 'adcdcde';
	actualmatch = string.match(pattern);
	expectedmatch = Array('adcdcde');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(91);
	pattern = /(ab|a)b*c/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc', 'ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(92);
	pattern = /((a)(b)c)(d)/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd', 'abc', 'a', 'b', 'd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(93);
	pattern = /[a-zA-Z_][a-zA-Z0-9_]*/;
	string = 'alpha';
	actualmatch = string.match(pattern);
	expectedmatch = Array('alpha');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(94);
	pattern = /^a(bc+|b[eh])g|.h$/;
	string = 'abh';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bh', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(95);
	pattern = /(bc+d$|ef*g.|h?i(j|k))/;
	string = 'effgz';
	actualmatch = string.match(pattern);

	expectedmatch = Array('effgz', 'effgz', undefined);

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(96);
	pattern = /(bc+d$|ef*g.|h?i(j|k))/;
	string = 'ij';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ij', 'ij', 'j');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(97);
	pattern = /(bc+d$|ef*g.|h?i(j|k))/;
	string = 'reffgz';
	actualmatch = string.match(pattern);

	expectedmatch = Array('effgz', 'effgz', undefined);

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(98);
	pattern = /((((((((((a))))))))))/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(99);
	pattern = /((((((((((a))))))))))\10/;
	string = 'aa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(100);
	pattern = /((((((((((a))))))))))/;
	string = 'a!';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(101);
	pattern = /(((((((((a)))))))))/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(102);
	pattern = /(.*)c(.*)/;
	string = 'abcde';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcde', 'ab', 'de');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(103);
	pattern = /abcd/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(104);
	pattern = /a(bc)d/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcd', 'bc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(105);
	pattern = /a[-]?c/;
	string = 'ac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ac');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(106);
	pattern = /(abc)\1/;
	string = 'abcabc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcabc', 'abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(107);
	pattern = /([a-c]*)\1/;
	string = 'abcabc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcabc', 'abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(108);
	pattern = /(a)|\1/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(109);
	pattern = /(([a-c])b*?\2)*/;
	string = 'ababbbcbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ababb', 'bb', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(110);
	pattern = /(([a-c])b*?\2){3}/;
	string = 'ababbbcbc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ababbbcbc', 'cbc', 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Can't refer to a capture before it's encountered & completed
	status = inSection(111);
	pattern = /((\3|b)\2(a)x)+/;
	string = 'aaaxabaxbaaxbbax';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bbax', 'bbax', 'b', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(112);
	pattern = /((\3|b)\2(a)){2,}/;
	string = 'bbaababbabaaaaabbaaaabba';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bbaaaabba', 'bba', 'b', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(113);
	pattern = /abc/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(114);
	pattern = /abc/i;
	string = 'XABCY';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(115);
	pattern = /abc/i;
	string = 'ABABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(116);
	pattern = /ab*c/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(117);
	pattern = /ab*bc/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(118);
	pattern = /ab*bc/i;
	string = 'ABBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(119);
	pattern = /ab*?bc/i;
	string = 'ABBBBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(120);
	pattern = /ab{0,}?bc/i;
	string = 'ABBBBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(121);
	pattern = /ab+?bc/i;
	string = 'ABBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(122);
	pattern = /ab+bc/i;
	string = 'ABBBBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(123);
	pattern = /ab{1,}?bc/i;
	string = 'ABBBBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(124);
	pattern = /ab{1,3}?bc/i;
	string = 'ABBBBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(125);
	pattern = /ab{3,4}?bc/i;
	string = 'ABBBBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(126);
	pattern = /ab??bc/i;
	string = 'ABBC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(127);
	pattern = /ab??bc/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(128);
	pattern = /ab{0,1}?bc/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(129);
	pattern = /ab??c/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(130);
	pattern = /ab{0,1}?c/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(131);
	pattern = /^abc$/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(132);
	pattern = /^abc/i;
	string = 'ABCC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(133);
	pattern = /abc$/i;
	string = 'AABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(134);
	pattern = /^/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(135);
	pattern = /$/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(136);
	pattern = /a.c/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(137);
	pattern = /a.c/i;
	string = 'AXC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AXC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(138);
	pattern = /a.*?c/i;
	string = 'AXYZC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AXYZC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(139);
	pattern = /a[bc]d/i;
	string = 'ABD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABD');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(140);
	pattern = /a[b-d]e/i;
	string = 'ACE';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ACE');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(141);
	pattern = /a[b-d]/i;
	string = 'AAC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(142);
	pattern = /a[-b]/i;
	string = 'A-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(143);
	pattern = /a[b-]/i;
	string = 'A-';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A-');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(144);
	pattern = /a]/i;
	string = 'A]';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A]');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl supports ] & ^] inside a [], ECMA does not
	status = inSection(145);
	pattern = /a[]]b/i;
	string = 'A]B';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A]B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(146);
	pattern = /a[^bc]d/i;
	string = 'AED';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AED');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(147);
	pattern = /a[^-b]c/i;
	string = 'ADC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ADC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl supports ] & ^] inside a [], ECMA does not
	status = inSection(148);
	pattern = /a[^]b]c/i;
	string = 'ADC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ADC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(149);
	pattern = /ab|cd/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(150);
	pattern = /ab|cd/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(151);
	pattern = /()ef/i;
	string = 'DEF';
	actualmatch = string.match(pattern);
	expectedmatch = Array('EF', '');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(152);
	pattern = /a\(b/i;
	string = 'A(B';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A(B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(153);
	pattern = /a\(*b/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(154);
	pattern = /a\(*b/i;
	string = 'A((B';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A((B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(155);
	pattern = /a\\b/i;
	string = 'A\\B';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A\\B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(156);
	pattern = /((a))/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(157);
	pattern = /(a)b(c)/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC', 'A', 'C');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(158);
	pattern = /a+b+c/i;
	string = 'AABBABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(159);
	pattern = /a{1,}b{1,}c/i;
	string = 'AABBABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(160);
	pattern = /a.+?c/i;
	string = 'ABCABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(161);
	pattern = /a.*?c/i;
	string = 'ABCABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(162);
	pattern = /a.{0,5}?c/i;
	string = 'ABCABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(163);
	pattern = /(a+|b)*/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB', 'B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(164);
	pattern = /(a+|b){0,}/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB', 'B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(165);
	pattern = /(a+|b)+/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB', 'B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(166);
	pattern = /(a+|b){1,}/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB', 'B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(167);
	pattern = /(a+|b)?/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(168);
	pattern = /(a+|b){0,1}/i;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(169);
	pattern = /(a+|b){0,1}?/i;
	string = 'AB';
	actualmatch = string.match(pattern);

	expectedmatch = Array('', undefined);

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(170);
	pattern = /[^ab]*/i;
	string = 'CDE';
	actualmatch = string.match(pattern);
	expectedmatch = Array('CDE');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(171);
	pattern = /([abc])*d/i;
	string = 'ABBBCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABBBCD', 'C');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(172);
	pattern = /([abc])*bcd/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(173);
	pattern = /a|b|c|d|e/i;
	string = 'E';
	actualmatch = string.match(pattern);
	expectedmatch = Array('E');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(174);
	pattern = /(a|b|c|d|e)f/i;
	string = 'EF';
	actualmatch = string.match(pattern);
	expectedmatch = Array('EF', 'E');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(175);
	pattern = /abcd*efg/i;
	string = 'ABCDEFG';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCDEFG');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(176);
	pattern = /ab*/i;
	string = 'XABYABBBZ';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(177);
	pattern = /ab*/i;
	string = 'XAYABBBZ';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(178);
	pattern = /(ab|cd)e/i;
	string = 'ABCDE';
	actualmatch = string.match(pattern);
	expectedmatch = Array('CDE', 'CD');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(179);
	pattern = /[abhgefdc]ij/i;
	string = 'HIJ';
	actualmatch = string.match(pattern);
	expectedmatch = Array('HIJ');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(180);
	pattern = /(abc|)ef/i;
	string = 'ABCDEF';
	actualmatch = string.match(pattern);
	expectedmatch = Array('EF', '');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(181);
	pattern = /(a|b)c*d/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('BCD', 'B');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(182);
	pattern = /(ab|ab*)bc/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(183);
	pattern = /a([bc]*)c*/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC', 'BC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(184);
	pattern = /a([bc]*)(c*d)/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD', 'BC', 'D');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(185);
	pattern = /a([bc]+)(c*d)/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD', 'BC', 'D');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(186);
	pattern = /a([bc]*)(c+d)/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD', 'B', 'CD');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(187);
	pattern = /a[bcd]*dcdcde/i;
	string = 'ADCDCDE';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ADCDCDE');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(188);
	pattern = /(ab|a)b*c/i;
	string = 'ABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABC', 'AB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(189);
	pattern = /((a)(b)c)(d)/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD', 'ABC', 'A', 'B', 'D');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(190);
	pattern = /[a-zA-Z_][a-zA-Z0-9_]*/i;
	string = 'ALPHA';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ALPHA');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(191);
	pattern = /^a(bc+|b[eh])g|.h$/i;
	string = 'ABH';
	actualmatch = string.match(pattern);
	expectedmatch = Array('BH', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(192);
	pattern = /(bc+d$|ef*g.|h?i(j|k))/i;
	string = 'EFFGZ';
	actualmatch = string.match(pattern);

	expectedmatch = Array('EFFGZ', 'EFFGZ', undefined);

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(193);
	pattern = /(bc+d$|ef*g.|h?i(j|k))/i;
	string = 'IJ';
	actualmatch = string.match(pattern);
	expectedmatch = Array('IJ', 'IJ', 'J');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(194);
	pattern = /(bc+d$|ef*g.|h?i(j|k))/i;
	string = 'REFFGZ';
	actualmatch = string.match(pattern);

	expectedmatch = Array('EFFGZ', 'EFFGZ', undefined);

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(195);
	pattern = /((((((((((a))))))))))/i;
	string = 'A';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(196);
	pattern = /((((((((((a))))))))))\10/i;
	string = 'AA';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AA', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(197);
	pattern = /((((((((((a))))))))))/i;
	string = 'A!';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(198);
	pattern = /(((((((((a)))))))))/i;
	string = 'A';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(199);
	pattern = /(?:(?:(?:(?:(?:(?:(?:(?:(?:(a))))))))))/i;
	string = 'A';
	actualmatch = string.match(pattern);
	expectedmatch = Array('A', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(200);
	pattern = /(?:(?:(?:(?:(?:(?:(?:(?:(?:(a|b|c))))))))))/i;
	string = 'C';
	actualmatch = string.match(pattern);
	expectedmatch = Array('C', 'C');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(201);
	pattern = /(.*)c(.*)/i;
	string = 'ABCDE';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCDE', 'AB', 'DE');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(202);
	pattern = /abcd/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(203);
	pattern = /a(bc)d/i;
	string = 'ABCD';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCD', 'BC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(204);
	pattern = /a[-]?c/i;
	string = 'AC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(205);
	pattern = /(abc)\1/i;
	string = 'ABCABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCABC', 'ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(206);
	pattern = /([a-c]*)\1/i;
	string = 'ABCABC';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ABCABC', 'ABC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(207);
	pattern = /a(?!b)./;
	string = 'abad';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ad');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(208);
	pattern = /a(?=d)./;
	string = 'abad';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ad');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(209);
	pattern = /a(?=c|d)./;
	string = 'abad';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ad');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(210);
	pattern = /a(?:b|c|d)(.)/;
	string = 'ace';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ace', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(211);
	pattern = /a(?:b|c|d)*(.)/;
	string = 'ace';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ace', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(212);
	pattern = /a(?:b|c|d)+?(.)/;
	string = 'ace';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ace', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(213);
	pattern = /a(?:b|c|d)+?(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acd', 'd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(214);
	pattern = /a(?:b|c|d)+(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdbe', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(215);
	pattern = /a(?:b|c|d){2}(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdb', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(216);
	pattern = /a(?:b|c|d){4,5}(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdb', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(217);
	pattern = /a(?:b|c|d){4,5}?(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcd', 'd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	// MODIFIED - ECMA has different rules for paren contents
	status = inSection(218);
	pattern = /((foo)|(bar))*/;
	string = 'foobar';
	actualmatch = string.match(pattern);
	//expectedmatch = Array('foobar', 'bar', 'foo', 'bar');
	expectedmatch = Array('foobar', 'bar', undefined, 'bar');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(219);
	pattern = /a(?:b|c|d){6,7}(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdbe', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(220);
	pattern = /a(?:b|c|d){6,7}?(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdbe', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(221);
	pattern = /a(?:b|c|d){5,6}(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdbe', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(222);
	pattern = /a(?:b|c|d){5,6}?(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdb', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(223);
	pattern = /a(?:b|c|d){5,7}(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdbe', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(224);
	pattern = /a(?:b|c|d){5,7}?(.)/;
	string = 'acdbcdbe';
	actualmatch = string.match(pattern);
	expectedmatch = Array('acdbcdb', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(225);
	pattern = /a(?:b|(c|e){1,2}?|d)+?(.)/;
	string = 'ace';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ace', 'c', 'e');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(226);
	pattern = /^(.+)?B/;
	string = 'AB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('AB', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* MODIFIED - ECMA has different rules for paren contents */
	status = inSection(227);
	pattern = /^([^a-z])|(\^)$/;
	string = '.';
	actualmatch = string.match(pattern);
	expectedmatch = Array('.', '.', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(228);
	pattern = /^[<>]&/;
	string = '<&OUT';
	actualmatch = string.match(pattern);
	expectedmatch = Array('<&');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Can't refer to a capture before it's encountered & completed
	status = inSection(229);
	pattern = /^(a\1?){4}$/;
	string = 'aaaaaaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaaaaaa', 'aaaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(230);
	pattern = /^(a(?(1)\1)){4}$/;
	string = 'aaaaaaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaaaaaa', 'aaaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(231);
	pattern = /((a{4})+)/;
	string = 'aaaaaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaaaa', 'aaaaaaaa', 'aaaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(232);
	pattern = /(((aa){2})+)/;
	string = 'aaaaaaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaaaa', 'aaaaaaaa', 'aaaa', 'aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(233);
	pattern = /(((a{2}){2})+)/;
	string = 'aaaaaaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaaaa', 'aaaaaaaa', 'aaaa', 'aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(234);
	pattern = /(?:(f)(o)(o)|(b)(a)(r))*/;
	string = 'foobar';
	actualmatch = string.match(pattern);
	expectedmatch = Array('foobar', 'f', 'o', 'o', 'b', 'a', 'r');
	//expectedmatch = Array('foobar', undefined, undefined, undefined, 'b', 'a', 'r');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA supports (?: (?= and (?! but doesn't support (?< etc.
	status = inSection(235);
	pattern = /(?<=a)b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(236);
	pattern = /(?<!c)b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(237);
	pattern = /(?<!c)b/;
	string = 'b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(238);
	pattern = /(?<!c)b/;
	string = 'b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(239);
	pattern = /(?:..)*a/;
	string = 'aba';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aba');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(240);
	pattern = /(?:..)*?a/;
	string = 'aba';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/*
	 * MODIFIED - ECMA has different rules for paren contents. Note
	 * this regexp has two non-capturing parens, and one capturing
	 *
	 * The issue: shouldn't the match be ['ab', undefined]? Because the
	 * '\1' matches the undefined value of the second iteration of the '*'
	 * (in which the 'b' part of the '|' matches). But Perl wants ['ab','b'].
	 *
	 * Answer: waldemar@netscape.com:
	 *
	 * The correct answer is ['ab', undefined].  Perl doesn't match
	 * ECMAScript here, and I'd say that Perl is wrong in this case.
	 */
	status = inSection(241);
	pattern = /^(?:b|a(?=(.)))*\1/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(242);
	pattern = /^(){3,5}/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('', '');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(243);
	pattern = /^(a+)*ax/;
	string = 'aax';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aax', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(244);
	pattern = /^((a|b)+)*ax/;
	string = 'aax';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aax', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(245);
	pattern = /^((a|bc)+)*ax/;
	string = 'aax';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aax', 'a', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* MODIFIED - ECMA has different rules for paren contents */
	status = inSection(246);
	pattern = /(a|x)*ab/;
	string = 'cab';
	actualmatch = string.match(pattern);
	//expectedmatch = Array('ab', '');
	expectedmatch = Array('ab', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(247);
	pattern = /(a)*ab/;
	string = 'cab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA doesn't support (?imsx or (?-imsx
	status = inSection(248);
	pattern = /(?:(?i)a)b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(249);
	pattern = /((?i)a)b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(250);
	pattern = /(?:(?i)a)b/;
	string = 'Ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('Ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(251);
	pattern = /((?i)a)b/;
	string = 'Ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('Ab', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(252);
	pattern = /(?i:a)b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(253);
	pattern = /((?i:a))b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(254);
	pattern = /(?i:a)b/;
	string = 'Ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('Ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(255);
	pattern = /((?i:a))b/;
	string = 'Ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('Ab', 'A');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(256);
	pattern = /(?:(?-i)a)b/i;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(257);
	pattern = /((?-i)a)b/i;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(258);
	pattern = /(?:(?-i)a)b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(259);
	pattern = /((?-i)a)b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(260);
	pattern = /(?:(?-i)a)b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(261);
	pattern = /((?-i)a)b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(262);
	pattern = /(?-i:a)b/i;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(263);
	pattern = /((?-i:a))b/i;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(264);
	pattern = /(?-i:a)b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(265);
	pattern = /((?-i:a))b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(266);
	pattern = /(?-i:a)b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(267);
	pattern = /((?-i:a))b/i;
	string = 'aB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aB', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(268);
	pattern = /((?s-i:a.))b/i;
	string = 'a\nB';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a\nB', 'a\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(269);
	pattern = /(?:c|d)(?:)(?:a(?:)(?:b)(?:b(?:))(?:b(?:)(?:b)))/;
	string = 'cabbbb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('cabbbb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(270);
	pattern = /(?:c|d)(?:)(?:aaaaaaaa(?:)(?:bbbbbbbb)(?:bbbbbbbb(?:))(?:bbbbbbbb(?:)(?:bbbbbbbb)))/;
	string = 'caaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('caaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(271);
	pattern = /(ab)\d\1/i;
	string = 'Ab4ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('Ab4ab', 'Ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(272);
	pattern = /(ab)\d\1/i;
	string = 'ab4Ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab4Ab', 'ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(273);
	pattern = /foo\w*\d{4}baz/;
	string = 'foobar1234baz';
	actualmatch = string.match(pattern);
	expectedmatch = Array('foobar1234baz');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(274);
	pattern = /x(~~)*(?:(?:F)?)?/;
	string = 'x~~';
	actualmatch = string.match(pattern);
	expectedmatch = Array('x~~', '~~');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl supports (?# but JS doesn't
	status = inSection(275);
	pattern = /^a(?#xxx){3}c/;
	string = 'aaac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaac');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* ECMA doesn't support (?< etc
	status = inSection(276);
	pattern = /(?<![cd])[ab]/;
	string = 'dbaacb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(277);
	pattern = /(?<!(c|d))[ab]/;
	string = 'dbaacb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(278);
	pattern = /(?<!cd)[ab]/;
	string = 'cdaccb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(279);
	pattern = /((?s)^a(.))((?m)^b$)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a\nb', 'a\n', '\n', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(280);
	pattern = /((?m)^b$)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(281);
	pattern = /(?m)^b/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(282);
	pattern = /(?m)^(b)/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(283);
	pattern = /((?m)^b)/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(284);
	pattern = /\n((?m)^b)/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('\nb', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(285);
	pattern = /((?s).)c(?!.)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('\nc', '\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(286);
	pattern = /((?s).)c(?!.)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('\nc', '\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(287);
	pattern = /((?s)b.)c(?!.)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b\nc', 'b\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(288);
	pattern = /((?s)b.)c(?!.)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b\nc', 'b\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(289);
	pattern = /((?m)^b)/;
	string = 'a\nb\nc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b', 'b');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* ECMA doesn't support (?(condition)
	status = inSection(290);
	pattern = /(?(1)b|a)/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(291);
	pattern = /(x)?(?(1)b|a)/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(292);
	pattern = /()?(?(1)b|a)/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(293);
	pattern = /()?(?(1)a|b)/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(294);
	pattern = /^(\()?blah(?(1)(\)))$/;
	string = '(blah)';
	actualmatch = string.match(pattern);
	expectedmatch = Array('(blah)', '(', ')');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(295);
	pattern = /^(\()?blah(?(1)(\)))$/;
	string = 'blah';
	actualmatch = string.match(pattern);
	expectedmatch = Array('blah');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(296);
	pattern = /^(\(+)?blah(?(1)(\)))$/;
	string = '(blah)';
	actualmatch = string.match(pattern);
	expectedmatch = Array('(blah)', '(', ')');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(297);
	pattern = /^(\(+)?blah(?(1)(\)))$/;
	string = 'blah';
	actualmatch = string.match(pattern);
	expectedmatch = Array('blah');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(298);
	pattern = /(?(?!a)b|a)/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(299);
	pattern = /(?(?=a)a|b)/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(300);
	pattern = /(?=(a+?))(\1ab)/;
	string = 'aaab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aab', 'a', 'aab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(301);
	pattern = /(\w+:)+/;
	string = 'one:';
	actualmatch = string.match(pattern);
	expectedmatch = Array('one:', 'one:');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA doesn't support (?< etc
	status = inSection(302);
	pattern = /$(?<=^(a))/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(303);
	pattern = /(?=(a+?))(\1ab)/;
	string = 'aaab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aab', 'a', 'aab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* MODIFIED - ECMA has different rules for paren contents */
	status = inSection(304);
	pattern = /([\w:]+::)?(\w+)$/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	//expectedmatch = Array('abcd', '', 'abcd');
	expectedmatch = Array('abcd', undefined, 'abcd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(305);
	pattern = /([\w:]+::)?(\w+)$/;
	string = 'xy:z:::abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('xy:z:::abcd', 'xy:z:::', 'abcd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(306);
	pattern = /^[^bcd]*(c+)/;
	string = 'aexycd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aexyc', 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(307);
	pattern = /(a*)b+/;
	string = 'caab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aab', 'aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* MODIFIED - ECMA has different rules for paren contents */
	status = inSection(308);
	pattern = /([\w:]+::)?(\w+)$/;
	string = 'abcd';
	actualmatch = string.match(pattern);
	//expectedmatch = Array('abcd', '', 'abcd');
	expectedmatch = Array('abcd', undefined, 'abcd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(309);
	pattern = /([\w:]+::)?(\w+)$/;
	string = 'xy:z:::abcd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('xy:z:::abcd', 'xy:z:::', 'abcd');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(310);
	pattern = /^[^bcd]*(c+)/;
	string = 'aexycd';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aexyc', 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA doesn't support (?>
	status = inSection(311);
	pattern = /(?>a+)b/;
	string = 'aaab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(312);
	pattern = /([[:]+)/;
	string = 'a:[b]:';
	actualmatch = string.match(pattern);
	expectedmatch = Array(':[', ':[');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(313);
	pattern = /([[=]+)/;
	string = 'a=[b]=';
	actualmatch = string.match(pattern);
	expectedmatch = Array('=[', '=[');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(314);
	pattern = /([[.]+)/;
	string = 'a.[b].';
	actualmatch = string.match(pattern);
	expectedmatch = Array('.[', '.[');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA doesn't have rules for [:
	status = inSection(315);
	pattern = /[a[:]b[:c]/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* ECMA doesn't support (?>
	status = inSection(316);
	pattern = /((?>a+)b)/;
	string = 'aaab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaab', 'aaab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(317);
	pattern = /(?>(a+))b/;
	string = 'aaab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaab', 'aaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(318);
	pattern = /((?>[^()]+)|\([^()]*\))+/;
	string = '((abc(ade)ufh()()x';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc(ade)ufh()()x', 'x');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(319);
	pattern = /\Z/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(320);
	pattern = /\z/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(321);
	pattern = /$/;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(322);
	pattern = /\Z/;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(323);
	pattern = /\z/;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(324);
	pattern = /$/;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(325);
	pattern = /\Z/;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(326);
	pattern = /\z/;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(327);
	pattern = /$/;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(328);
	pattern = /\Z/m;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(329);
	pattern = /\z/m;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(330);
	pattern = /$/m;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(331);
	pattern = /\Z/m;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(332);
	pattern = /\z/m;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(333);
	pattern = /$/m;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(334);
	pattern = /\Z/m;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(335);
	pattern = /\z/m;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(336);
	pattern = /$/m;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(337);
	pattern = /a\Z/;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* $ only matches end of input unless multiline
	status = inSection(338);
	pattern = /a$/;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(339);
	pattern = /a\Z/;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(340);
	pattern = /a\z/;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(341);
	pattern = /a$/;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(342);
	pattern = /a$/m;
	string = 'a\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(343);
	pattern = /a\Z/m;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(344);
	pattern = /a$/m;
	string = 'b\na\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(345);
	pattern = /a\Z/m;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(346);
	pattern = /a\z/m;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(347);
	pattern = /a$/m;
	string = 'b\na';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(348);
	pattern = /aa\Z/;
	string = 'b\naa\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* $ only matches end of input unless multiline
	status = inSection(349);
	pattern = /aa$/;
	string = 'b\naa\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(350);
	pattern = /aa\Z/;
	string = 'b\naa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(351);
	pattern = /aa\z/;
	string = 'b\naa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(352);
	pattern = /aa$/;
	string = 'b\naa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(353);
	pattern = /aa$/m;
	string = 'aa\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(354);
	pattern = /aa\Z/m;
	string = 'b\naa\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(355);
	pattern = /aa$/m;
	string = 'b\naa\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(356);
	pattern = /aa\Z/m;
	string = 'b\naa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(357);
	pattern = /aa\z/m;
	string = 'b\naa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(358);
	pattern = /aa$/m;
	string = 'b\naa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(359);
	pattern = /ab\Z/;
	string = 'b\nab\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* $ only matches end of input unless multiline
	status = inSection(360);
	pattern = /ab$/;
	string = 'b\nab\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(361);
	pattern = /ab\Z/;
	string = 'b\nab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(362);
	pattern = /ab\z/;
	string = 'b\nab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(363);
	pattern = /ab$/;
	string = 'b\nab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(364);
	pattern = /ab$/m;
	string = 'ab\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(365);
	pattern = /ab\Z/m;
	string = 'b\nab\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(366);
	pattern = /ab$/m;
	string = 'b\nab\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(367);
	pattern = /ab\Z/m;
	string = 'b\nab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(368);
	pattern = /ab\z/m;
	string = 'b\nab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(369);
	pattern = /ab$/m;
	string = 'b\nab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(370);
	pattern = /abb\Z/;
	string = 'b\nabb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* $ only matches end of input unless multiline
	status = inSection(371);
	pattern = /abb$/;
	string = 'b\nabb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(372);
	pattern = /abb\Z/;
	string = 'b\nabb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(373);
	pattern = /abb\z/;
	string = 'b\nabb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(374);
	pattern = /abb$/;
	string = 'b\nabb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(375);
	pattern = /abb$/m;
	string = 'abb\nb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(376);
	pattern = /abb\Z/m;
	string = 'b\nabb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(377);
	pattern = /abb$/m;
	string = 'b\nabb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Perl has \Z has end-of-line, ECMA doesn't
	status = inSection(378);
	pattern = /abb\Z/m;
	string = 'b\nabb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(379);
	pattern = /abb\z/m;
	string = 'b\nabb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(380);
	pattern = /abb$/m;
	string = 'b\nabb';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abb');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(381);
	pattern = /(^|x)(c)/;
	string = 'ca';
	actualmatch = string.match(pattern);
	expectedmatch = Array('c', '', 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(382);
	pattern = /foo.bart/;
	string = 'foo.bart';
	actualmatch = string.match(pattern);
	expectedmatch = Array('foo.bart');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(383);
	pattern = /^d[x][x][x]/m;
	string = 'abcd\ndxxx';
	actualmatch = string.match(pattern);
	expectedmatch = Array('dxxx');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(384);
	pattern = /tt+$/;
	string = 'xxxtt';
	actualmatch = string.match(pattern);
	expectedmatch = Array('tt');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA spec says that each atom in a range must be a single character
	status = inSection(385);
	pattern = /([a-\d]+)/;
	string = 'za-9z';
	actualmatch = string.match(pattern);
	expectedmatch = Array('9', '9');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(386);
	pattern = /([\d-z]+)/;
	string = 'a0-za';
	actualmatch = string.match(pattern);
	expectedmatch = Array('0-z', '0-z');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	/* ECMA doesn't support [:
	status = inSection(387);
	pattern = /([a-[:digit:]]+)/;
	string = 'za-9z';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a-9', 'a-9');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(388);
	pattern = /([[:digit:]-z]+)/;
	string = '=0-z=';
	actualmatch = string.match(pattern);
	expectedmatch = Array('0-z', '0-z');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(389);
	pattern = /([[:digit:]-[:alpha:]]+)/;
	string = '=0-z=';
	actualmatch = string.match(pattern);
	expectedmatch = Array('0-z', '0-z');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(390);
	pattern = /(\d+\.\d+)/;
	string = '3.1415926';
	actualmatch = string.match(pattern);
	expectedmatch = Array('3.1415926', '3.1415926');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(391);
	pattern = /\.c(pp|xx|c)?$/i;
	string = 'IO.c';
	actualmatch = string.match(pattern);
	expectedmatch = Array('.c', undefined);

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(392);
	pattern = /(\.c(pp|xx|c)?$)/i;
	string = 'IO.c';
	actualmatch = string.match(pattern);

	expectedmatch = Array('.c', '.c', undefined);


	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(393);
	pattern = /(^|a)b/;
	string = 'ab';
	actualmatch = string.match(pattern);
	expectedmatch = Array('ab', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(394);
	pattern = /^([ab]*?)(b)?(c)$/;
	string = 'abac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abac', 'aba', undefined, 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(395);
	pattern = /^(?:.,){2}c/i;
	string = 'a,b,c';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a,b,c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(396);
	pattern = /^(.,){2}c/i;
	string = 'a,b,c';
	actualmatch = string.match(pattern);
	expectedmatch =  Array('a,b,c', 'b,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(397);
	pattern = /^(?:[^,]*,){2}c/;
	string = 'a,b,c';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a,b,c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(398);
	pattern = /^([^,]*,){2}c/;
	string = 'a,b,c';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a,b,c', 'b,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(399);
	pattern = /^([^,]*,){3}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(400);
	pattern = /^([^,]*,){3,}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(401);
	pattern = /^([^,]*,){0,3}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(402);
	pattern = /^([^,]{1,3},){3}d/i;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(403);
	pattern = /^([^,]{1,3},){3,}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(404);
	pattern = /^([^,]{1,3},){0,3}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(405);
	pattern = /^([^,]{1,},){3}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(406);
	pattern = /^([^,]{1,},){3,}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(407);
	pattern = /^([^,]{1,},){0,3}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(408);
	pattern = /^([^,]{0,3},){3}d/i;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(409);
	pattern = /^([^,]{0,3},){3,}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(410);
	pattern = /^([^,]{0,3},){0,3}d/;
	string = 'aaa,b,c,d';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaa,b,c,d', 'c,');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* ECMA doesn't support \A
	status = inSection(411);
	pattern = /(?!\A)x/m;
	string = 'a\nxb\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(412);
	pattern = /^(a(b)?)+$/;
	string = 'aba';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aba', 'a', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(413);
	pattern = /^(aa(bb)?)+$/;
	string = 'aabbaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aabbaa', 'aa', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(414);
	pattern = /^.{9}abc.*\n/m;
	string = '123\nabcabcabcabc\n';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcabcabcabc\n');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(415);
	pattern = /^(a)?a$/;
	string = 'a';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a', undefined);
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(416);
	pattern = /^(a\1?)(a\1?)(a\2?)(a\3?)$/;
	string = 'aaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaa', 'a', 'aa', 'a', 'aa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/* Can't refer to a capture before it's encountered & completed
	status = inSection(417);
	pattern = /^(a\1?){4}$/;
	string = 'aaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaa', 'aaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	status = inSection(418);
	pattern = /^(0+)?(?:x(1))?/;
	string = 'x1';
	actualmatch = string.match(pattern);
	expectedmatch = Array('x1', undefined, '1');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(419);
	pattern = /^([0-9a-fA-F]+)(?:x([0-9a-fA-F]+)?)(?:x([0-9a-fA-F]+))?/;
	string = '012cxx0190';
	actualmatch = string.match(pattern);
	expectedmatch = Array('012cxx0190', '012c', undefined, '0190');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(420);
	pattern = /^(b+?|a){1,2}c/;
	string = 'bbbac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bbbac', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(421);
	pattern = /^(b+?|a){1,2}c/;
	string = 'bbbbac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bbbbac', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(422);
	pattern = /((?:aaaa|bbbb)cccc)?/;
	string = 'aaaacccc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaacccc', 'aaaacccc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(423);
	pattern = /((?:aaaa|bbbb)cccc)?/;
	string = 'bbbbcccc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('bbbbcccc', 'bbbbcccc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
