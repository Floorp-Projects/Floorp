/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------

var BUGNUMBER = 315436;
var summary = 'In strict mode, setting a read-only property should generate a warning';

printBugNumber(BUGNUMBER);
printStatus (summary);

enterFunc (String (BUGNUMBER));

// should throw an error in strict mode
var actual = '';
var expect = '"length" is read-only';
var status = summary + ': Throw if STRICT and WERROR is enabled';

if (!options().match(/strict/))
{
  options('strict');
}
if (!options().match(/werror/))
{
  options('werror');
}

try
{
  var s = new String ('abc');
  s.length = 0;
}
catch (e)
{
  actual = e.message;
}

reportCompare(expect, actual, status);

// should not throw an error if in strict mode and WERROR is false

actual = 'did not throw';
expect = 'did not throw';
var status = summary + ': Do not throw if STRICT is enabled and WERROR is disabled';

// toggle werror off
options('werror');

try
{
  s.length = 0;
}
catch (e)
{
  actual = e.message;
}

reportCompare(expect, actual, status);

// should not throw an error if not in strict mode

actual = 'did not throw';
expect = 'did not throw';
var status = summary + ': Do not throw if not in strict mode';

// toggle strict off
options('strict');

try
{
  s.length = 0;
}
catch (e)
{
  actual = e.message;
}

reportCompare(expect, actual, status);
