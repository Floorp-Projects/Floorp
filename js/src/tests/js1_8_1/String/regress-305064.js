// |reftest| fails
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 305064;
var summary = 'Tests the trim, trimRight  and trimLeft methods';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var trimMethods = ['trim', 'trimLeft', 'trimRight'];

/** List from ES 3.1 Recommendation for String.trim (bug 305064) **/
var whitespace = [
  {s : '\u0009', t : 'HORIZONTAL TAB'},
  {s : '\u000B', t : 'VERTICAL TAB'},
  {s : '\u000C', t : 'FORMFEED'},
  {s : '\u0020', t : 'SPACE'},
  {s : '\u00A0', t : 'NO-BREAK SPACE'},
  {s : '\u1680', t : 'OGHAM SPACE MARK'},
  {s : '\u180E', t : 'MONGOLIAN VOWEL SEPARATOR'},
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
  {s : '\u200B', t : 'ZERO WIDTH SPACE (category Cf)'}
  ];

for (var j = 0; j < trimMethods.length; ++j)
{
  var str;

  var method = trimMethods[j];

  if (typeof String.prototype[method] == 'undefined')
  {
    reportCompare(true, true, 'Test skipped. String.prototype.' + method + ' is not supported');
    continue;
  }

  print('Test empty string.');
  str      = '';
  expected = '';
  actual   = str[method]();
  reportCompare(expected, actual, '"' + toPrinted(str) + '".' + method + '()');                        

  print('Test string with no whitespace.');
  str      = 'a';
  expected = 'a';
  actual   = str[method]();
  reportCompare(expected, actual, '"' + toPrinted(str) + '".' + method + '()');                        

  for (var i = 0; i < whitespace.length; ++i)
  {
    var v = whitespace[i].s;
    var t = whitespace[i].t;
    v = v + v + v;

    print('=======================================');
    print('Test ' + method + ' with with only whitespace. : ' + t);
    str      = v;
    expected = '';
    actual   = str[method]();
    reportCompare(expected, actual, t + ':' + '"' + toPrinted(str) + '".' + method + '()');

    print('Test ' + method + ' with with no leading or trailing whitespace. : ' + t);
    str      = 'a' + v + 'b';
    expected = str;
    actual   = str[method]();
    reportCompare(expected, actual, t + ':' + '"' + toPrinted(str) + '".' + method + '()');

    print('Test ' + method + ' with with leading whitespace. : ' + t);
    str = v + 'a';
    switch(method)
    {
    case 'trim':
      expected = 'a';
      break;
    case 'trimLeft':
      expected = 'a';
      break;
    case 'trimRight':
      expected = str;
      break;
    }
    actual   = str[method]();
    reportCompare(expected, actual, t + ':' + '"' + toPrinted(str) + '".' + method + '()');
      
    print('Test ' + method + ' with with trailing whitespace. : ' + t);
    str = 'a' + v;
    switch(method)
    {
    case 'trim':
      expected = 'a';
      break;
    case 'trimLeft':
      expected = str;
      break;
    case 'trimRight':
      expected = 'a';
      break;
    }
    actual = str[method]();
    reportCompare(expected, actual, t + ':' + '"' + toPrinted(str) + '".' + method + '()');

    print('Test ' + method + ' with with leading and trailing whitepace.');
    str = v + 'a' + v;
    switch(method)
    {
    case 'trim':
      expected = 'a';
      break;
    case 'trimLeft':
      expected = 'a' + v;
      break;
    case 'trimRight':
      expected = v + 'a';
      break;
    }
    actual = str[method]();
    reportCompare(expected, actual, t + ':' + '"' + toPrinted(str) + '".' + method + '()');
      
  }
}

