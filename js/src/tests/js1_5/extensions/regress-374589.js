/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 374589;
var summary = 'Do not assert decompiling try { } catch(x if true) { } ' +
  'catch(y) { } finally { this.a.b; }';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f = function () {
    try { } catch(x if true) { } catch(y) { } finally { this.a.b; } };

  expect = 'function () {\n\
    try { } catch(x if true) { } catch(y) { } finally { this.a.b; } }';

  actual = f + '';
  compareSource(expect, actual, summary);
}
