/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is
* Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2008
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): Dave Mandelin
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */
 
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
"\u202f", "\u205f", "\u3000" ];
var line_terminators = [ "\u2028", "\u2029", "\u000a", "\u000d" ];
var space_chars = [].concat(spaces, line_terminators);
 
var non_space_chars = [ "\u200b", "\u200c", "\u200d",
"\ufeff" ];
 
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
