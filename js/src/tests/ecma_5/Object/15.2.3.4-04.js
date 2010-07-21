/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = '15.2.3.4-04.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 518663;
var summary = 'Object.getOwnPropertyNames: regular expression objects';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var actual = Object.getOwnPropertyNames(/a/);
var expected = ["lastIndex", "source", "global", "ignoreCase", "multiline"];

for (var i = 0; i < expected.length; i++)
{
  reportCompare(actual.indexOf(expected[i]) >= 0, true,
                expected[i] + " should be a property name on a RegExp");
}

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
