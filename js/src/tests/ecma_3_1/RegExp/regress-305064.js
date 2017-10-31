/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 305064;
var summary = 'CharacterClassEscape \\s';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  /** List from ES 3.1 Recommendation for String.trim (bug 305064) **/
  var whitespace = [
    {s : '\u0009', t : 'HORIZONTAL TAB'},
    {s : '\u000B', t : 'VERTICAL TAB'},
    {s : '\u000C', t : 'FORMFEED'},
    {s : '\u0020', t : 'SPACE'},
    {s : '\u00A0', t : 'NO-BREAK SPACE'},
    {s : '\u1680', t : 'OGHAM SPACE MARK'},
    {s : '\u2000', t : 'EN QUAD'},
    {s : '\u2001', t : 'EM QUAD'},
    {s : '\u2002', t : 'EN SPACE'},
    {s : '\u2003', t : 'EM SPACE'},
    {s : '\u2004', t : 'THREE-PER-EM SPACE'},
    {s : '\u2005', t : 'FOUR-PER-EM SPACE'},
    {s : '\u2006', t : 'SIX-PER-EM SPACE'},
    {s : '\u2007', t : 'FIGURE SPACE'},
    {s : '\u2008', t : 'PUNCTUATION SPACE'},
    {s : '\u2009', t : 'THIN SPACE'},
    {s : '\u200A', t : 'HAIR SPACE'},
    {s : '\u202F', t : 'NARROW NO-BREAK SPACE'},
    {s : '\u205F', t : 'MEDIUM MATHEMATICAL SPACE'},
    {s : '\u3000', t : 'IDEOGRAPHIC SPACE'},
    {s : '\u000A', t : 'LINE FEED OR NEW LINE'},
    {s : '\u000D', t : 'CARRIAGE RETURN'},
    {s : '\u2028', t : 'LINE SEPARATOR'},
    {s : '\u2029', t : 'PARAGRAPH SEPARATOR'},
    ];

  for (var i = 0; i < whitespace.length; ++i)
  {
    var v = whitespace[i];
    reportCompare(true, !!(/\s/.test(v.s)), 'Is ' + v.t + ' a space');
  }
}
