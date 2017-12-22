/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 371932;
var summary = 'ES4 Regular Expression /y flag';
var actual = '';
var expect = '';

print('See http://developer.mozilla.org/es4/proposals/extend_regexps.html#y_flag');

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var c;
  var s = '123456';

  print('Test global flag.');

  var g = /(1)/g;
  expect = 'captures: 1,1; RegExp.leftContext: ""; RegExp.rightContext: "234561"';
  actual = 'captures: ' + g.exec('1234561') +
    '; RegExp.leftContext: "' + RegExp.leftContext +
    '"; RegExp.rightContext: "' + RegExp.rightContext + '"';
  reportCompare(expect, actual, summary + ' - /(1)/g.exec("1234561") first call');

  expect = 'captures: 1,1; RegExp.leftContext: "123456"; RegExp.rightContext: ""';
  actual = 'captures: ' + g.exec('1234561') +
    '; RegExp.leftContext: "' + RegExp.leftContext +
    '"; RegExp.rightContext: "' + RegExp.rightContext + '"';
  reportCompare(expect, actual, summary + ' - /(1)/g.exec("1234561") second call');
  var y = /(1)/y;
 
  print('Test sticky flag.');

  /*
   * calls to reportCompare invoke regular expression matches which interfere
   * with the test of the sticky flag. Collect expect and actual values prior
   * to calling reportCompare. Note setting y = /(1)/y resets the lastIndex etc.
   */

  var y = /(1)/y;
  var expect4 = 'captures: 1,1; RegExp.leftContext: ""; RegExp.rightContext: "234561"';
  var actual4 = 'captures: ' + y.exec('1234561') +
    '; RegExp.leftContext: "' + RegExp.leftContext +
    '"; RegExp.rightContext: "' + RegExp.rightContext + '"';

  var expect5 = 'captures: null; RegExp.leftContext: ""; RegExp.rightContext: "234561"';
  var actual5 = 'captures: ' + y.exec('1234561') +
    '; RegExp.leftContext: "' + RegExp.leftContext +
    '"; RegExp.rightContext: "' + RegExp.rightContext + '"';

  reportCompare(expect4, actual4, summary + ' - /(1)/y.exec("1234561") first call');
  reportCompare(expect5, actual5, summary + ' - /(1)/y.exec("1234561") second call');

  var y = /(1)/y;

  reportCompare(expect5, actual5, summary);

  y = /(1)/y;
  var expect6 = 'captures: 1,1; RegExp.leftContext: ""; RegExp.rightContext: "123456"';
  var actual6 = 'captures: ' + y.exec('1123456') +
    '; RegExp.leftContext: "' + RegExp.leftContext +
    '"; RegExp.rightContext: "' + RegExp.rightContext + '"';

  var expect7 = 'captures: 1,1; RegExp.leftContext: "1"; RegExp.rightContext: "23456"';
  var actual7 = 'captures: ' + y.exec('1123456') +
    '; RegExp.leftContext: "' + RegExp.leftContext +
    '"; RegExp.rightContext: "' + RegExp.rightContext + '"';

  reportCompare(expect6, actual6, summary + ' - /(1)/y.exec("1123456") first call');
  reportCompare(expect7, actual7, summary + ' - /(1)/y.exec("1123456") second call');

  var y = /(1)/y;

  reportCompare(expect, actual, summary);
}
