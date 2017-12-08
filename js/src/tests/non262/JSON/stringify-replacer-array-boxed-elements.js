// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'stringify-replacer-array-boxed-elements.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 648471;
var summary = "Boxed-string/number objects in replacer arrays";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var S = new String(3);
var N = new Number(4);

assertEq(JSON.stringify({ 3: 3, 4: 4 }, [S]),
         '{"3":3}');
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [N]),
         '{"4":4}');

Number.prototype.toString = function() { return 3; };
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [N]),
         '{"3":3}');

String.prototype.toString = function() { return 4; };
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [S]),
         '{"4":4}');

Number.prototype.toString = function() { return new String(42); };
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [N]),
         '{"4":4}');

String.prototype.toString = function() { return new Number(17); };
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [S]),
         '{"3":3}');

Number.prototype.toString = null;
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [N]),
         '{"4":4}');

String.prototype.toString = null;
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [S]),
         '{"3":3}');

Number.prototype.valueOf = function() { return 17; };
assertEq(JSON.stringify({ 4: 4, 17: 17 }, [N]),
         '{"17":17}');

String.prototype.valueOf = function() { return 42; };
assertEq(JSON.stringify({ 3: 3, 42: 42 }, [S]),
         '{"42":42}');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
