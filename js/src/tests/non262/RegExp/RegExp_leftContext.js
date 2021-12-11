/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_leftContext.js
   Description:  'Tests RegExps leftContext property'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: leftContext';

writeHeaderToLog('Executing script: RegExp_leftContext.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abc123xyz'.match(/123/); RegExp.leftContext
'abc123xyz'.match(/123/);
new TestCase ( "'abc123xyz'.match(/123/); RegExp.leftContext",
	       'abc', RegExp.leftContext);

// 'abc123xyz'.match(/456/); RegExp.leftContext
'abc123xyz'.match(/456/);
new TestCase ( "'abc123xyz'.match(/456/); RegExp.leftContext",
	       'abc', RegExp.leftContext);

// 'abc123xyz'.match(/abc123xyz/); RegExp.leftContext
'abc123xyz'.match(/abc123xyz/);
new TestCase ( "'abc123xyz'.match(/abc123xyz/); RegExp.leftContext",
	       '', RegExp.leftContext);

// 'xxxx'.match(/$/); RegExp.leftContext
'xxxx'.match(/$/);
new TestCase ( "'xxxx'.match(/$/); RegExp.leftContext",
	       'xxxx', RegExp.leftContext);

// 'test'.match(/^/); RegExp.leftContext
'test'.match(/^/);
new TestCase ( "'test'.match(/^/); RegExp.leftContext",
	       '', RegExp.leftContext);

// 'xxxx'.match(new RegExp('$')); RegExp.leftContext
'xxxx'.match(new RegExp('$'));
new TestCase ( "'xxxx'.match(new RegExp('$')); RegExp.leftContext",
	       'xxxx', RegExp.leftContext);

// 'test'.match(new RegExp('^')); RegExp.leftContext
'test'.match(new RegExp('^'));
new TestCase ( "'test'.match(new RegExp('^')); RegExp.leftContext",
	       '', RegExp.leftContext);

test();
