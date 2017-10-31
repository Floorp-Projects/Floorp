/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     strictEquality.js
   Description:  'This tests the operator ==='

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE = 'operator "==="';

writeHeaderToLog('Executing script: strictEquality.js');
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "('8' === 8)                              ",
	      false,  ('8' === 8));

new TestCase( "(8 === 8)                                ",
	      true,   (8 === 8));

new TestCase( "(8 === true)                             ",
	      false,  (8 === true));

new TestCase( "(new String('') === new String(''))      ",
	      false,  (new String('') === new String('')));

new TestCase( "(new Boolean(true) === new Boolean(true))",
	      false,  (new Boolean(true) === new Boolean(true)));

var anObject = { one:1 , two:2 };

new TestCase( "(anObject === anObject)                  ",
	      true,  (anObject === anObject));

new TestCase( "(anObject === { one:1 , two:2 })         ",
	      false,  (anObject === { one:1 , two:2 }));

new TestCase( "({ one:1 , two:2 } === anObject)         ",
	      false,  ({ one:1 , two:2 } === anObject));

new TestCase( "(null === null)                          ",
	      true,  (null === null));

new TestCase( "(null === 0)                             ",
	      false,  (null === 0));

new TestCase( "(true === !false)                        ",
	      true,  (true === !false));

test();

