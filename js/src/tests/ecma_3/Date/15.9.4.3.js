/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 363578;
var summary = '15.9.4.3 - Date.UTC edge-case arguments.';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  //

  expect = 31;
  actual = (new Date(Date.UTC(2006, 0, 0)).getUTCDate());
  reportCompare(expect, actual, summary + ': date 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0, 0)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds 0');

  //

  expect = 30;
  actual = (new Date(Date.UTC(2006, 0, -1)).getUTCDate());
  reportCompare(expect, actual, summary + ': date -1');

  expect = 23;
  actual = (new Date(Date.UTC(2006, 0, 0, -1)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours -1');

  expect = 59;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, -1)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes -1');

  expect = 59;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, -1)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds -1');

  expect = 999;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0, -1)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds -1');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, undefined)).getUTCDate());
  reportCompare(expect, actual, summary + ': date undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, undefined)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, undefined)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, undefined)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, undefined)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds undefined');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, {})).getUTCDate());
  reportCompare(expect, actual, summary + ': date {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, {})).getUTCHours());
  reportCompare(expect, actual, summary + ': hours {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, {})).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, {})).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, {})).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds {}');

  //

  expect = 31;
  actual = (new Date(Date.UTC(2006, 0, null)).getUTCDate());
  reportCompare(expect, actual, summary + ': date null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, null)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, null)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, null)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0, null)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds null');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, Infinity)).getUTCDate());
  reportCompare(expect, actual, summary + ': date Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, Infinity)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, Infinity)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, Infinity)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, Infinity)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds Infinity');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, -Infinity)).getUTCDate());
  reportCompare(expect, actual, summary + ': date -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, -Infinity)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, -Infinity)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, -Infinity)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, -Infinity)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds -Infinity');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, NaN)).getUTCDate());
  reportCompare(expect, actual, summary + ': date NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, NaN)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, NaN)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, NaN)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, NaN)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds NaN');
}
