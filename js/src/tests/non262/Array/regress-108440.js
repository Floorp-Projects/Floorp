// |reftest| skip-if(Android) -- bug - nsIDOMWindow.crypto throws NS_ERROR_NOT_IMPLEMENTED on Android
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date: 30 October 2001
 * SUMMARY: Regression test for bug 108440
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=108440
 *
 * We shouldn't crash trying to add an array as an element of itself (!)
 *
 * Brendan: "...it appears that Array.prototype.toString is unsafe,
 * and what's more, ECMA-262 Edition 3 has no helpful words about
 * avoiding recursive death on a cycle."
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 108440;
var summary = "Shouldn't crash trying to add an array as an element of itself";
var self = this;
var temp = '';

printBugNumber(BUGNUMBER);
printStatus(summary);

/*
 * Explicit test:
 */
var a=[];
temp = (a[a.length]=a);

/*
 * Implicit test (one of the properties of |self| is |a|)
 */
a=[];
for(var prop in self)
{
  temp = prop;
  temp = (a[a.length] = self[prop]);
}

/*
 * Stressful explicit test
 */
a=[];
for (var i=0; i<10; i++)
{
  a[a.length] = a;
}

/*
 * Test toString()
 */
a=[];
for (var i=0; i<10; i++)
{
  a[a.length] = a.toString();
}

/*
 * Test toSource() - but Rhino doesn't have this, so try...catch it
 */
a=[];
try
{
  for (var i=0; i<10; i++)
  {
    a[a.length] = a.toSource();
  }
}
catch(e)
{
}

reportCompare('No Crash', 'No Crash', '');
