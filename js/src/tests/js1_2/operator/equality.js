// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     equality.js
   Description:  'This tests the operator =='

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'operator "=="';

writeHeaderToLog('Executing script: equality.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// the following two tests are incorrect
//new TestCase( SECTION, "(new String('') == new String(''))       ",
//                                            true,   (new String('') == new String('')));

//new TestCase( SECTION, "(new Boolean(true) == new Boolean(true)) ",
//                                            true,   (new Boolean(true) == new Boolean(true)));

new TestCase( SECTION, "(new String('x') == 'x')                 ",
	      false,   (new String('x') == 'x'));

new TestCase( SECTION, "('x' == new String('x'))                 ",
	      false,   ('x' == new String('x')));


test();

