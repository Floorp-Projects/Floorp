/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
var gTestfile = 'regexp-space-character-class.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 514808;
var summary = 'Correct space character class in regexes';
var actual = '';
var expect = '';
 
 
//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------
 
function test()
{
enterFunc ('test');
printBugNumber(BUGNUMBER);
printStatus (summary);
 
var spaces = [ "\u0009", "\u000b", "\u000c", "\u0020", "\u00a0", "\u1680",
"\u180e", "\u2000", "\u2001", "\u2002", "\u2003", "\u2004",
"\u2005", "\u2006", "\u2007", "\u2008", "\u2009", "\u200a",
"\u202f", "\u205f", "\u3000", "\ufeff" ];
var line_terminators = [ "\u2028", "\u2029", "\u000a", "\u000d" ];
var space_chars = [].concat(spaces, line_terminators);
 
var non_space_chars = [ "\u200b", "\u200c", "\u200d" ];
 
var chars = [].concat(space_chars, non_space_chars);
var is_space = [].concat(space_chars.map(function(ch) { return true; }),
non_space_chars.map(function(ch) { return false; }));
var expect = is_space.join(',');
 
var actual = chars.map(function(ch) { return /\s/.test(ch); }).join(',');
reportCompare(expect, actual, summary);
 
jit(true);
var actual = chars.map(function(ch) { return /\s/.test(ch); }).join(',');
reportCompare(expect, actual, summary);
jit(false);
 
exitFunc ('test');
}
