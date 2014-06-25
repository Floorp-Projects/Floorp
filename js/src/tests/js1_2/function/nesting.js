/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     nesting.js
   Description:  'This tests the nesting of functions'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'functions: nesting';

writeHeaderToLog('Executing script: nesting.js');
writeHeaderToLog( SECTION + " "+ TITLE);

function outer_func(x)
{
  var y = "outer";

  new TestCase( SECTION, "outer:x    ",
		1111,  x);
  new TestCase( SECTION, "outer:y    ",
		'outer', y);
  function inner_func(x)
  {
    var y = "inner";
    new TestCase( SECTION, "inner:x    ",
		  2222,  x);
    new TestCase( SECTION, "inner:y    ",
		  'inner', y);
  };

  inner_func(2222);
  new TestCase( SECTION, "outer:x    ",
		1111,  x);
  new TestCase( SECTION, "outer:y    ",
		'outer', y);
}

outer_func(1111);

test();

