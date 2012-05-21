/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_dollar_number.js
   Description:  'Tests RegExps $1, ..., $9 properties'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "What\'s new in JavaScript 1.2"';
var VERSION = 'no version';
var TITLE   = 'RegExp: $1, ..., $9';
var BUGNUMBER="123802";

startTest();
writeHeaderToLog('Executing script: RegExp_dollar_number.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$1
'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/);
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$1",
	       'abcdefghi', RegExp.$1);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$2
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$2",
	       'bcdefgh', RegExp.$2);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$3
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$3",
	       'cdefg', RegExp.$3);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$4
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$4",
	       'def', RegExp.$4);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$5
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$5",
	       'e', RegExp.$5);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$6
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$6",
	       '', RegExp.$6);

var a_to_z = 'abcdefghijklmnopqrstuvwxyz';
var regexp1 = /(a)b(c)d(e)f(g)h(i)j(k)l(m)n(o)p(q)r(s)t(u)v(w)x(y)z/
  // 'abcdefghijklmnopqrstuvwxyz'.match(/(a)b(c)d(e)f(g)h(i)j(k)l(m)n(o)p(q)r(s)t(u)v(w)x(y)z/); RegExp.$1
  a_to_z.match(regexp1);

new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$1",
	       'a', RegExp.$1);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$2",
	       'c', RegExp.$2);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$3",
	       'e', RegExp.$3);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$4",
	       'g', RegExp.$4);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$5",
	       'i', RegExp.$5);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$6",
	       'k', RegExp.$6);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$7",
	       'm', RegExp.$7);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$8",
	       'o', RegExp.$8);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$9",
	       'q', RegExp.$9);
/*
  new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$10",
  's', RegExp.$10);
*/
test();
