/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_leftContext_as_array.js
   Description:  'Tests RegExps leftContext property (same tests as RegExp_leftContext.js but using $`)'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: $`';

writeHeaderToLog('Executing script: RegExp_leftContext_as_array.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abc123xyz'.match(/123/); RegExp['$`']
'abc123xyz'.match(/123/);
new TestCase ( "'abc123xyz'.match(/123/); RegExp['$`']",
	       'abc', RegExp['$`']);

// 'abc123xyz'.match(/456/); RegExp['$`']
'abc123xyz'.match(/456/);
new TestCase ( "'abc123xyz'.match(/456/); RegExp['$`']",
	       'abc', RegExp['$`']);

// 'abc123xyz'.match(/abc123xyz/); RegExp['$`']
'abc123xyz'.match(/abc123xyz/);
new TestCase ( "'abc123xyz'.match(/abc123xyz/); RegExp['$`']",
	       '', RegExp['$`']);

// 'xxxx'.match(/$/); RegExp['$`']
'xxxx'.match(/$/);
new TestCase ( "'xxxx'.match(/$/); RegExp['$`']",
	       'xxxx', RegExp['$`']);

// 'test'.match(/^/); RegExp['$`']
'test'.match(/^/);
new TestCase ( "'test'.match(/^/); RegExp['$`']",
	       '', RegExp['$`']);

// 'xxxx'.match(new RegExp('$')); RegExp['$`']
'xxxx'.match(new RegExp('$'));
new TestCase ( "'xxxx'.match(new RegExp('$')); RegExp['$`']",
	       'xxxx', RegExp['$`']);

// 'test'.match(new RegExp('^')); RegExp['$`']
'test'.match(new RegExp('^'));
new TestCase ( "'test'.match(new RegExp('^')); RegExp['$`']",
	       '', RegExp['$`']);

test();
