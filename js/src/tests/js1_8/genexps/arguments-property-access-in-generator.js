/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 721322;
var summary = 'Allow f.arguments in generator expressions';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

eval("(function() { return (f.arguments for (x in [1])); })()");
eval("(function() { var f = { arguments: 12 }; return [f.arguments for (x in [1])]; })()");


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
