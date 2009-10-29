/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Bob Clary
 */

var gTestfile = 'regress-169559.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 169559;
var summary = 'Global vars should not be more than 2.5 times slower than local';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var starttime;
var stoptime;
var globaltime = 0;
var localtime  = 0;
var maxratio   = 2.5;
var ratio;

var globalvar;
var globaltotal = 0;

printStatus("Testing global variables");
starttime = new Date();
for (globalvar = 0; globalvar < 100000; globalvar++)
{
  globaltotal += 1;
}
stoptime = new Date();
globaltime = stoptime - starttime;

printStatus("Testing local variables");
starttime= new Date();
local();
stoptime = new Date();
localtime = stoptime - starttime;

ratio = globaltime/localtime;
printStatus("Ratio of global to local time " + ratio.toFixed(3));

expect = true;
actual = (ratio < maxratio);
summary += ', Ratio: ' + ratio + ' '; 
reportCompare(expect, actual, summary);

function local()
{
  var localvar;
  var localtotal = 0;

  for (localvar = 0; localvar < 100000; localvar++)
  {
    localtotal += 1;
  }

}
