/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    06 Mar 2002
 * SUMMARY: Testing cloned function objects
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=127557
 *
 * Before this bug was fixed, this testcase would error when run:
 *
 *             ReferenceError: h_peer is not defined
 *
 * The line |g.prototype = new Object| below is essential: this is
 * what was confusing the engine in its attempt to look up h_peer
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 127557;
var summary = 'Testing cloned function objects';
var cnCOMMA = ',';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

if (typeof clone == 'function')
{
  status = inSection(1);
  var f = evaluate("(function(x, y) {\n" +
                   "    function h() { return h_peer(); }\n" +
                   "    function h_peer() { return (x + cnCOMMA + y); }\n" +
                   "    return h;\n" +
                   "})");
  var g = clone(f);
  g.prototype = new Object;
  var h = g(5,6);
  actual = h();
  expect = '5,6';
  addThis();
}
else
{
  reportCompare('Test not run', 'Test not run', 'shell only test requires clone()');
}



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}

