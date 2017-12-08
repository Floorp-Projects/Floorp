/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     everything.js
   Description:  'Tests regular expressions'

   Author:       Nick Lerissa
   Date:         March 24, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp';

writeHeaderToLog('Executing script: everything.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'Sally and Fred are sure to come.'.match(/^[a-z\s]*/i)
new TestCase ( "'Sally and Fred are sure to come'.match(/^[a-z\\s]*/i)",
	       String(["Sally and Fred are sure to come"]), String('Sally and Fred are sure to come'.match(/^[a-z\s]*/i)));

// 'test123W+xyz'.match(new RegExp('^[a-z]*[0-9]+[A-Z]?.(123|xyz)$'))
new TestCase ( "'test123W+xyz'.match(new RegExp('^[a-z]*[0-9]+[A-Z]?.(123|xyz)$'))",
	       String(["test123W+xyz","xyz"]), String('test123W+xyz'.match(new RegExp('^[a-z]*[0-9]+[A-Z]?.(123|xyz)$'))));

// 'number one 12365 number two 9898'.match(/(\d+)\D+(\d+)/)
new TestCase ( "'number one 12365 number two 9898'.match(/(\d+)\D+(\d+)/)",
	       String(["12365 number two 9898","12365","9898"]), String('number one 12365 number two 9898'.match(/(\d+)\D+(\d+)/)));

var simpleSentence = /(\s?[^\!\?\.]+[\!\?\.])+/;
// 'See Spot run.'.match(simpleSentence)
new TestCase ( "'See Spot run.'.match(simpleSentence)",
	       String(["See Spot run.","See Spot run."]), String('See Spot run.'.match(simpleSentence)));

// 'I like it. What's up? I said NO!'.match(simpleSentence)
new TestCase ( "'I like it. What's up? I said NO!'.match(simpleSentence)",
	       String(["I like it. What's up? I said NO!",' I said NO!']), String('I like it. What\'s up? I said NO!'.match(simpleSentence)));

// 'the quick brown fox jumped over the lazy dogs'.match(/((\w+)\s*)+/)
new TestCase ( "'the quick brown fox jumped over the lazy dogs'.match(/((\\w+)\\s*)+/)",
	       String(['the quick brown fox jumped over the lazy dogs','dogs','dogs']),String('the quick brown fox jumped over the lazy dogs'.match(/((\w+)\s*)+/)));

test();
