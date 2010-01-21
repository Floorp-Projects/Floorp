/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'redeclare-var-non-writable-property.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 539488;
var summary =
  '|var| statements for existing, read-only/permanent properties should not ' +
  'be errors';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var undefined;

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
