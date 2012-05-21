/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     charCodeAt.js
   Description:  'This tests new String object method: charCodeAt'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'String:charCodeAt';

writeHeaderToLog('Executing script: charCodeAt.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var aString = new String("tEs5");

new TestCase( SECTION, "aString.charCodeAt(-2)", NaN, aString.charCodeAt(-2));
new TestCase( SECTION, "aString.charCodeAt(-1)", NaN, aString.charCodeAt(-1));
new TestCase( SECTION, "aString.charCodeAt( 0)", 116, aString.charCodeAt( 0));
new TestCase( SECTION, "aString.charCodeAt( 1)",  69, aString.charCodeAt( 1));
new TestCase( SECTION, "aString.charCodeAt( 2)", 115, aString.charCodeAt( 2));
new TestCase( SECTION, "aString.charCodeAt( 3)",  53, aString.charCodeAt( 3));
new TestCase( SECTION, "aString.charCodeAt( 4)", NaN, aString.charCodeAt( 4));
new TestCase( SECTION, "aString.charCodeAt( 5)", NaN, aString.charCodeAt( 5));
new TestCase( SECTION, "aString.charCodeAt( Infinity)", NaN, aString.charCodeAt( Infinity));
new TestCase( SECTION, "aString.charCodeAt(-Infinity)", NaN, aString.charCodeAt(-Infinity));
//new TestCase( SECTION, "aString.charCodeAt(  )", 116, aString.charCodeAt( ));

test();

