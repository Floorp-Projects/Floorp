/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 354541;
var summary = 'Regression to standard class constructors in case labels';
var actual = '';
var expect = '';


printBugNumber(BUGNUMBER);
printStatus (summary + ': top level');

String.prototype.trim = function() { print('hallo'); };

String.prototype.trim = function() { return 'hallo'; };

const S = String;
const Sp = String.prototype;

expect = 'hallo';
var expectStringInvariant = true
  var actualStringInvariant;
var expectStringPrototypeInvariant = true;
var actualStringPrototypeInvariant;

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
  reportCompare("Script not defined, Test skipped.",
                "Script not defined, Test skipped.",
                summary);
}
else
{
  var s = Script('var tmp = function(o) { switch(o) { case String: case 1: return ""; } }; actualStringInvariant = (String === S); actualStringPrototypeInvariant = (String.prototype === Sp); actual = "".trim();');
  try
  {
    s();
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'trim() returned');
  reportCompare(expectStringInvariant, actualStringInvariant,
                'String invariant');
  reportCompare(expectStringPrototypeInvariant,
                actualStringPrototypeInvariant,
                'String.prototype invariant');

}
 
