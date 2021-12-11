/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 412926;
var summary = 'JS_ValueToId(cx, JSVAL_NULL) should return atom for "null" string';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  actual = expect = 'No Errors';

  var obj = { 'null': 1 };

  var errors = [];

  if (!obj.hasOwnProperty(null))
    errors.push('null property is not owned');

  if (!obj.propertyIsEnumerable(null))
    errors.push('null property is not enumerable');

  var getter_was_called = false;
  obj.__defineGetter__(null, function() { getter_was_called = true; return 1; });
  obj['null'];

  if (!getter_was_called)
    errors.push('getter was not assigned to the null property');

  var setter_was_called = false;
  obj.__defineSetter__(null, function() { setter_was_called = true; });
  obj['null'] = 2;

  if (!setter_was_called)
    errors.push('setter was not assigned to the null property');

  if (errors.length)
    actual = errors.join('; ');

  gc();

  reportCompare(expect, actual, summary);
}
