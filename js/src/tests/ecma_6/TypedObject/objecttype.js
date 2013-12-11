// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 917454;
var summary = 'objecttype';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var T = TypedObject;

function runTests() {
  var Point = new T.ArrayType(T.float32, 3);
  var Line = new T.StructType({from: Point, to: Point});
  var Lines = new T.ArrayType(Line, 3);

  var lines = new Lines([
    {from: [1, 2, 3], to: [4, 5, 6]},
    {from: [7, 8, 9], to: [10, 11, 12]},
    {from: [13, 14, 15], to: [16, 17, 18]}
  ]);

  assertEq(T.objectType(lines), Lines);
  assertEq(T.objectType(lines[0]), Line);
  assertEq(T.objectType(lines[0].from[0]), T.float64);
  assertEq(T.objectType(""), T.String);
  assertEq(T.objectType({}), T.Object);
  assertEq(T.objectType([]), T.Object);
  assertEq(T.objectType(function() { }), T.Object);
  assertEq(T.objectType(undefined), T.Any);

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


