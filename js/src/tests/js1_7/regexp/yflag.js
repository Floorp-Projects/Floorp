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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
  enterFunc ('test');
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

  exitFunc ('test');
}
