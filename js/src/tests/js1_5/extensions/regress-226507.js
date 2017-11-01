/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    24 Nov 2003
 * SUMMARY: Testing for recursion check in js_EmitTree
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=226507
 * Igor's comments:
 *
 * "For example, with N in the test  set to 35, I got on my RedHat
 * Linux 10 box a segmentation fault from js after setting the stack limit
 * to 100K. When I set the stack limit to 20K I still got the segmentation fault.
 * Only after -s was changed to 15K, too-deep recursion was detected:
 *

 ~/w/js/x> ulimit -s
 100
 ~/w/js/x> js  fintest.js
 Segmentation fault
 ~/w/js/x> js -S $((20*1024)) fintest.js
 Segmentation fault
 ~/w/js/x> js -S $((15*1024)) fintest.js
 fintest.js:19: InternalError: too much recursion

 *
 * After playing with numbers it seems that while processing try/finally the
 * recursion in js_Emit takes 10 times more space the corresponding recursion
 * in the parser."
 *
 *
 * Note the use of the new -S option to the JS shell to limit stack size.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=225061. This in turn
 * can be passed to the JS shell by the test driver's -o option, as in:
 *
 * perl jsDriver.pl -e smdebug -fTEST.html -o "-S 100" -l js1_5/Regress
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 226507;
var summary = 'Testing for recursion check in js_EmitTree';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * With stack limit 100K on Linux debug build even N=30 already can cause
 * stack overflow; use 35 to trigger it for sure.
 */
var N = 350;

var counter = 0;
function f()
{
  ++counter;
}


/*
 * Example: if N were 3, this is what |source|
 * would end up looking like:
 *
 *     try { f(); } finally {
 *     try { f(); } finally {
 *     try { f(); } finally {
 *     f(1,1,1,1);
 *     }}}
 *
 */
var source = "".concat(
  repeat_str("try { f(); } finally {\n", N),
  "f(",
  repeat_str("1,", N),
  "1);\n",
  repeat_str("}", N));

// Repeat it for additional stress testing
source += source;

/*
 * In Rhino, eval() always uses interpreted mode.
 * To use compiled mode, use Script.exec() instead.
 */
if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
  expect = actual = 0;
}
else
{
  try
  {
    var script = Script(source);
    script();


    status = inSection(1);
    actual = counter;
    expect = (N + 1) * 2;
  }
  catch(ex)
  {
    actual = ex + '';
  }
}
addThis();


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function repeat_str(str, repeat_count)
{
  var arr = new Array(--repeat_count);
  while (repeat_count != 0)
    arr[--repeat_count] = str;
  return str.concat.apply(str, arr);
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
