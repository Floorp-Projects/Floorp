// |reftest| skip-if(!xulRuntime.shell)
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    11 Nov 2002
 * SUMMARY: JS shouldn't crash on extraneous args to str.match(), etc.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=179524
 *
 * Note that when testing str.replace(), we have to be careful if the first
 * argument provided to str.replace() is not a regexp object. ECMA-262 says
 * it is NOT converted to one, unlike the case for str.match(), str.search().
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=83293#c21. This means
 * we have to be careful how we test meta-characters in the first argument
 * to str.replace(), if that argument is a string -
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 179524;
var summary = "Don't crash on extraneous arguments to str.match(), etc.";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

str = 'ABC abc';
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
 * Now try the same thing with str.search()
 */
status = inSection(5);
actual = str.search(re);
expect = -1;
addThis();

status = inSection(6);
actual = str.search(re, 'i');
expect = -1;
addThis();

status = inSection(7);
actual = str.search(re, 'g', '');
expect = -1;
addThis();

status = inSection(8);
actual = str.search(re, 'z', new Object(), new Date());
expect = -1;
addThis();


/*
 * Now try the same thing with str.replace()
 */
status = inSection(9);
actual = str.replace(re, 'Z');
expect = str;
addThis();

status = inSection(10);
actual = str.replace(re, 'Z', 'i');
expect = str;
addThis();

status = inSection(11);
actual = str.replace(re, 'Z', 'g', '');
expect = str;
addThis();

status = inSection(12);
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
status = inSection(13);
actual = str.match('a').toString();
expect = str.match(/a/).toString();
addThis();

status = inSection(14);
actual = str.match('a', 'i').toString();
expect = str.match(/a/).toString();
addThis();

status = inSection(15);
actual = str.match('a', 'ig').toString();
expect = str.match(/a/).toString();
addThis();

status = inSection(16);
actual = str.match('\\s', 'm').toString();
expect = str.match(/\s/).toString();
addThis();


/*
 * Now try the previous three cases with extraneous parameters
 */
status = inSection(17);
actual = str.match('a', 'i', 'g').toString();
expect = str.match(/a/).toString();
addThis();

status = inSection(18);
actual = str.match('a', 'ig', new Object()).toString();
expect = str.match(/a/).toString();
addThis();

status = inSection(19);
actual = str.match('\\s', 'm', 999).toString();
expect = str.match(/\s/).toString();
addThis();

status = inSection(20);
actual = str.match('a', 'z').toString();
expect = str.match(/a/).toString();
addThis();



/*
 * Now test str.search() where the first argument is not a regexp object.
 * The same considerations as above apply -
 *
 * Reference: http://bugzilla.mozilla.org/show_bug.cgi?id=179524#c16
 */
status = inSection(21);
actual = str.search('a');
expect = str.search(/a/);
addThis();

status = inSection(22);
actual = str.search('a', 'i');
expect = str.search(/a/);
addThis();

status = inSection(23);
actual = str.search('a', 'ig');
expect = str.search(/a/);
addThis();

status = inSection(24);
actual = str.search('\\s', 'm');
expect = str.search(/\s/);
addThis();


/*
 * Now try the previous three cases with extraneous parameters
 */
status = inSection(25);
actual = str.search('a', 'i', 'g');
expect = str.search(/a/);
addThis();

status = inSection(26);
actual = str.search('a', 'ig', new Object());
expect = str.search(/a/);
addThis();

status = inSection(27);
actual = str.search('\\s', 'm', 999);
expect = str.search(/\s/);
addThis();

status = inSection(28);
actual = str.search('a', 'z');
expect = str.search(/a/);
addThis();



/*
 * Now test str.replace() where the first argument is not a regexp object.
 * The same considerations as above apply, EXCEPT for meta-characters.
 * See introduction to testcase above. References:
 *
 * http://bugzilla.mozilla.org/show_bug.cgi?id=179524#c16
 * http://bugzilla.mozilla.org/show_bug.cgi?id=83293#c21
 */
status = inSection(29);
actual = str.replace('a', 'Z');
expect = str.replace(/a/, 'Z');
addThis();

status = inSection(30);
actual = str.replace('a', 'Z', 'i');
expect = str.replace(/a/, 'Z');
addThis();

status = inSection(31);
actual = str.replace('a', 'Z', 'ig');
expect = str.replace(/a/, 'Z');
addThis();

status = inSection(32);
actual = str.replace('\\s', 'Z', 'm'); //<--- NO!!! No meta-characters 1st arg!
actual = str.replace(' ', 'Z', 'm');   //<--- Have to do this instead
expect = str.replace(/\s/, 'Z');
addThis();


/*
 * Now try the previous three cases with extraneous parameters
 */
status = inSection(33);
actual = str.replace('a', 'Z', 'i', 'g');
expect = str.replace(/a/, 'Z');
addThis();

status = inSection(34);
actual = str.replace('a', 'Z', 'ig', new Object());
expect = str.replace(/a/, 'Z');
addThis();

status = inSection(35);
actual = str.replace('\\s', 'Z', 'm', 999); //<--- NO meta-characters 1st arg!
actual = str.replace(' ', 'Z', 'm', 999);   //<--- Have to do this instead
expect = str.replace(/\s/, 'Z');
addThis();

status = inSection(36);
actual = str.replace('a', 'Z', 'z');
expect = str.replace(/a/, 'Z');
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
