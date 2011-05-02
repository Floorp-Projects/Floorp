// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'stringify-replacer-array-skipped-element.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 648471;
var summary =
  "Better/more correct handling for replacer arrays with getter array index " +
  "properties";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(JSON.stringify({ 3: 3, 4: 4 },
                        ["3", { toString: function() { return "4" } }]),
         '{"3":3}');

assertEq(JSON.stringify({ 3: 3, true: 4 }, ["3", true]),
         '{"3":3}');

assertEq(JSON.stringify({ 3: 3, true: 4 }, ["3", "true", true]),
         '{"3":3,"true":4}');

assertEq(JSON.stringify({ 3: 3, true: 4 }, ["3", true, "true"]),
         '{"3":3,"true":4}');

assertEq(JSON.stringify({ 3: 3, false: 4 }, ["3", false]),
         '{"3":3}');

assertEq(JSON.stringify({ 3: 3, false: 4 }, ["3", "false", false]),
         '{"3":3,"false":4}');

assertEq(JSON.stringify({ 3: 3, false: 4 }, ["3", false, "false"]),
         '{"3":3,"false":4}');

assertEq(JSON.stringify({ 3: 3, undefined: 4 }, ["3", undefined]),
         '{"3":3}');

assertEq(JSON.stringify({ 3: 3, undefined: 4 }, ["3", "undefined", undefined]),
         '{"3":3,"undefined":4}');

assertEq(JSON.stringify({ 3: 3, undefined: 4 }, ["3", undefined, "undefined"]),
         '{"3":3,"undefined":4}');

assertEq(JSON.stringify({ 3: 3, null: 4 }, ["3", null]),
         '{"3":3}');

assertEq(JSON.stringify({ 3: 3, null: 4 }, ["3", "null", null]),
         '{"3":3,"null":4}');

assertEq(JSON.stringify({ 3: 3, null: 4 }, ["3", null, "null"]),
         '{"3":3,"null":4}');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
