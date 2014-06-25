/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    10 February 2003
 * SUMMARY: Object.toSource() recursion should check stack overflow
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=192465
 *
 * MODIFIED: 27 February 2003
 *
 * We are adding an early return to this testcase, since it is causing
 * big problems on Linux RedHat8! For a discussion of this issue, see
 * http://bugzilla.mozilla.org/show_bug.cgi?id=174341#c24 and following.
 *
 *
 * MODIFIED: 20 March 2003
 *
 * Removed the early return and changed |N| below from 1000 to 90.
 * Note |make_deep_nest(N)| returns an object graph of length N(N+1).
 * So the graph has now been reduced from 1,001,000 to 8190.
 *
 * With this reduction, the bug still manifests on my WinNT and Linux
 * boxes (crash due to stack overflow). So the testcase is again of use
 * on those boxes. At the same time, Linux RedHat8 boxes can now run
 * the test in a reasonable amount of time.
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 192465;
var summary = 'Object.toSource() recursion should check stack overflow';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * We're just testing that this script will compile and run.
 * Set both |actual| and |expect| to a dummy value.
 */
status = inSection(1);
var N = 90;
try
{
  make_deep_nest(N);
}
catch (e)
{
  // An exception is OK, as the runtime can throw one in response to too deep
  // recursion. We haven't crashed; good! Continue on to set the dummy values -
}
actual = 1;
expect = 1;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


/*
 * EXAMPLE:
 *
 * If the global variable |N| is 2, then for |level| == 0, 1, 2, the return
 * value of this function will be toSource() of these objects, respectively:
 *
 * {next:{next:END}}
 * {next:{next:{next:{next:END}}}}
 * {next:{next:{next:{next:{next:{next:END}}}}}}
 *
 */
function make_deep_nest(level)
{
  var head = {};
  var cursor = head;

  for (var i=0; i!=N; ++i)
  {
    cursor.next = {};
    cursor = cursor.next;
  }

  cursor.toSource = function()
    {
      if (level != 0)
	return make_deep_nest(level - 1);
      return "END";
    }

  return head.toSource();
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
