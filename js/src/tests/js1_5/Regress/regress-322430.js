/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 322430;
var summary = 'Remove deprecated with statement warning';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

options('strict');
options('werror');

expect = 'No Warning';

try
{
  var obj = {foo: 'baz'};
 
  // this must either be top level or must be
  // evald since there is a bug in older versions
  // that suppresses the |with| warning inside of a
  // try catch block. doh!
  eval('with (obj) { foo; }');

  actual = 'No Warning';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary);
