/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 167658;
var summary = 'Do not crash due to js_NewRegExp initialization';
var actual = 'No Crash';
var expect = 'No Crash';
printBugNumber(BUGNUMBER);
printStatus (summary);

var UBOUND=100;
for (var j=0; j<UBOUND; j++)
{
  'Apfelkiste, Apfelschale'.replace('Apfel', function()
				    {
				      for (var i = 0; i < arguments.length; i++)
					printStatus(i+': '+arguments[i]);
				      return 'Bananen';
				    });

  printStatus(j);
}
 
reportCompare(expect, actual, summary);

