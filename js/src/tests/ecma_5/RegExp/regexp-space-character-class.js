/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
var gTestfile = 'regexp-space-character-class.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 514808;
var summary = 'Correct space character class in regexes';
 
 
//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------
 
function test()
{
printBugNumber(BUGNUMBER);
printStatus (summary);

// NOTE: White space and line terminators are now tested in
// ecma_6/RegExp/character-class-escape-s.js.

var non_space_chars = [ "\u200b", "\u200c", "\u200d" ];

reportCompare(non_space_chars.some(function(ch){ return /\s/.test(ch); }), false, summary);
}
