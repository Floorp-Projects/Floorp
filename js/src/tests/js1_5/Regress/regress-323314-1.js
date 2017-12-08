/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 323314;
var summary = 'JSMSG_EQUAL_AS_ASSIGN in js.msg should be JSEXN_SYNTAXERR';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (!options().match(/strict/))
{
  options('strict');
}
if (!options().match(/werror/))
{
  options('werror');
}

var xyzzy;

expect = 'SyntaxError';

try
{
  eval('if (xyzzy=1) printStatus(xyzzy);');

  actual = 'No Warning';
}
catch(ex)
{
  actual = ex.name;
  printStatus(ex + '');
}

reportCompare(expect, actual, summary);
