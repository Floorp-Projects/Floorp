/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     ignoreCase.js
   Description:  'Tests RegExp attribute ignoreCase'

   Author:       Nick Lerissa
   Date:         March 13, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'RegExp: ignoreCase';

writeHeaderToLog('Executing script: ignoreCase.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// /xyz/i.ignoreCase
new TestCase ( SECTION, "/xyz/i.ignoreCase",
	       true, /xyz/i.ignoreCase);

// /xyz/.ignoreCase
new TestCase ( SECTION, "/xyz/.ignoreCase",
	       false, /xyz/.ignoreCase);

// 'ABC def ghi'.match(/[a-z]+/ig)
new TestCase ( SECTION, "'ABC def ghi'.match(/[a-z]+/ig)",
	       String(["ABC","def","ghi"]), String('ABC def ghi'.match(/[a-z]+/ig)));

// 'ABC def ghi'.match(/[a-z]+/i)
new TestCase ( SECTION, "'ABC def ghi'.match(/[a-z]+/i)",
	       String(["ABC"]), String('ABC def ghi'.match(/[a-z]+/i)));

// 'ABC def ghi'.match(/([a-z]+)/ig)
new TestCase ( SECTION, "'ABC def ghi'.match(/([a-z]+)/ig)",
	       String(["ABC","def","ghi"]), String('ABC def ghi'.match(/([a-z]+)/ig)));

// 'ABC def ghi'.match(/([a-z]+)/i)
new TestCase ( SECTION, "'ABC def ghi'.match(/([a-z]+)/i)",
	       String(["ABC","ABC"]), String('ABC def ghi'.match(/([a-z]+)/i)));

// 'ABC def ghi'.match(/[a-z]+/)
new TestCase ( SECTION, "'ABC def ghi'.match(/[a-z]+/)",
	       String(["def"]), String('ABC def ghi'.match(/[a-z]+/)));

// (new RegExp('xyz','i')).ignoreCase
new TestCase ( SECTION, "(new RegExp('xyz','i')).ignoreCase",
	       true, (new RegExp('xyz','i')).ignoreCase);

// (new RegExp('xyz')).ignoreCase
new TestCase ( SECTION, "(new RegExp('xyz')).ignoreCase",
	       false, (new RegExp('xyz')).ignoreCase);

// 'ABC def ghi'.match(new RegExp('[a-z]+','ig'))
new TestCase ( SECTION, "'ABC def ghi'.match(new RegExp('[a-z]+','ig'))",
	       String(["ABC","def","ghi"]), String('ABC def ghi'.match(new RegExp('[a-z]+','ig'))));

// 'ABC def ghi'.match(new RegExp('[a-z]+','i'))
new TestCase ( SECTION, "'ABC def ghi'.match(new RegExp('[a-z]+','i'))",
	       String(["ABC"]), String('ABC def ghi'.match(new RegExp('[a-z]+','i'))));

// 'ABC def ghi'.match(new RegExp('([a-z]+)','ig'))
new TestCase ( SECTION, "'ABC def ghi'.match(new RegExp('([a-z]+)','ig'))",
	       String(["ABC","def","ghi"]), String('ABC def ghi'.match(new RegExp('([a-z]+)','ig'))));

// 'ABC def ghi'.match(new RegExp('([a-z]+)','i'))
new TestCase ( SECTION, "'ABC def ghi'.match(new RegExp('([a-z]+)','i'))",
	       String(["ABC","ABC"]), String('ABC def ghi'.match(new RegExp('([a-z]+)','i'))));

// 'ABC def ghi'.match(new RegExp('[a-z]+'))
new TestCase ( SECTION, "'ABC def ghi'.match(new RegExp('[a-z]+'))",
	       String(["def"]), String('ABC def ghi'.match(new RegExp('[a-z]+'))));

test();
