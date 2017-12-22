/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 327170;
var summary = 'Reuse of RegExp in string.replace(rx.compile(...), function() { rx.compile(...); }) causes a crash';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var g_rx = /(?:)/;

"this is a test-string".replace(g_rx.compile("test", "g"),
				function()
				{
				  // re-use of the g_rx RegExp object,
				  // that's currently in use by the replace fn.
				  g_rx.compile("string", "g");
				});
 
reportCompare(expect, actual, summary);
