/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     do_while.js
   Description:  'This tests the new do_while loop'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'statements: do_while';

writeHeaderToLog('Executing script: do_while.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var done = false;
var x = 0;
do
{
  if (x++ == 3) done = true;
} while (!done);

new TestCase( SECTION, "do_while ",
	      4, x);

//load('d:/javascript/tests/output/statements/do_while.js')
test();

