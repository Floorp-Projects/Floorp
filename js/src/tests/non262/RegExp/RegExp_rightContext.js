/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_rightContext.js
   Description:  'Tests RegExps rightContext property'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: rightContext';

writeHeaderToLog('Executing script: RegExp_rightContext.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abc123xyz'.match(/123/); RegExp.rightContext
'abc123xyz'.match(/123/);
new TestCase ( "'abc123xyz'.match(/123/); RegExp.rightContext",
	       'xyz', RegExp.rightContext);

// 'abc123xyz'.match(/456/); RegExp.rightContext
'abc123xyz'.match(/456/);
new TestCase ( "'abc123xyz'.match(/456/); RegExp.rightContext",
	       'xyz', RegExp.rightContext);

// 'abc123xyz'.match(/abc123xyz/); RegExp.rightContext
'abc123xyz'.match(/abc123xyz/);
new TestCase ( "'abc123xyz'.match(/abc123xyz/); RegExp.rightContext",
	       '', RegExp.rightContext);

// 'xxxx'.match(/$/); RegExp.rightContext
'xxxx'.match(/$/);
new TestCase ( "'xxxx'.match(/$/); RegExp.rightContext",
	       '', RegExp.rightContext);

// 'test'.match(/^/); RegExp.rightContext
'test'.match(/^/);
new TestCase ( "'test'.match(/^/); RegExp.rightContext",
	       'test', RegExp.rightContext);

// 'xxxx'.match(new RegExp('$')); RegExp.rightContext
'xxxx'.match(new RegExp('$'));
new TestCase ( "'xxxx'.match(new RegExp('$')); RegExp.rightContext",
	       '', RegExp.rightContext);

// 'test'.match(new RegExp('^')); RegExp.rightContext
'test'.match(new RegExp('^'));
new TestCase ( "'test'.match(new RegExp('^')); RegExp.rightContext",
	       'test', RegExp.rightContext);

test();
