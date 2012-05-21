/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    13 Oct 2003
 * SUMMARY: Make our f.caller property match IE's wrt f.apply and f.call
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=222029
 *
 * Below, when gg calls f via |f.call|, we have this call chain:
 *
 *          calls                                  calls
 *   gg() --------->  Function.prototype.call()  --------->  f()
 *
 *
 * The question this bug addresses is, "What should we say |f.caller| is?"
 *
 * Before this fix, SpiderMonkey said it was |Function.prototype.call|.
 * After this fix, SpiderMonkey emulates IE and says |gg| instead.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 222029;
var summary = "Make our f.caller property match IE's wrt f.apply and f.call";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function f()
{
  return f.caller.p ;
}


/*
 * Call |f| directly
 */
function g()
{
  return f();
}
g.p = "hello";


/*
 * Call |f| via |f.call|
 */
function gg()
{
  return f.call(this);
}
gg.p = "hello";


/*
 * Call |f| via |f.apply|
 */
function ggg()
{
  return f.apply(this);
}
ggg.p = "hello";


/*
 * Shadow |p| on |Function.prototype.call|, |Function.prototype.apply|.
 * In Sections 2 and 3 below, we no longer expect to recover this value -
 */
Function.prototype.call.p = "goodbye";
Function.prototype.apply.p = "goodbye";



status = inSection(1);
actual = g();
expect = "hello";
addThis();

status = inSection(2);
actual = gg();
expect = "hello";
addThis();

status = inSection(3);
actual = ggg();
expect = "hello";
addThis();




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
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
