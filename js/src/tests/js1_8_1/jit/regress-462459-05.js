/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 462459;
var summary = 'TM: trace new Array()';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


if (!this.tracemonkey || this.tracemonkey.adaptive)
{
  expect = actual = 'Test skipped due to lack of tracemonkey jitstats';
  reportCompare(expect, actual, summary);
}
else
{

  expect = 'recorder started, recorder not aborted, trace completed';
  actual = '';

  var recorderStartedStart = this.tracemonkey.recorderStarted;
  var recorderAbortedStart = this.tracemonkey.recorderAborted;
  var traceCompletedStart  = this.tracemonkey.traceCompleted;


  for (var i = 0; i < RUNLOOP; i++)
  {
    new Array();
  }


  var recorderStartedEnd = this.tracemonkey.recorderStarted;
  var recorderAbortedEnd = this.tracemonkey.recorderAborted;
  var traceCompletedEnd  = this.tracemonkey.traceCompleted;

  if (recorderStartedEnd > recorderStartedStart)
  {
    actual = 'recorder started, ';
  }
  else
  {
    actual = 'recorder not started, ';
  }

  if (recorderAbortedEnd > recorderAbortedStart)
  {
    actual += 'recorder aborted, ';
  }
  else
  {
    actual += 'recorder not aborted, ';
  }

  if (traceCompletedEnd > traceCompletedStart)
  {
    actual += 'trace completed';
  }
  else
  {
    actual += 'trace not completed';
  }

  reportCompare(expect, actual, summary);
}

