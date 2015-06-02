// |reftest| skip-if(!this.hasOwnProperty("TypedObject")||!this.hasOwnProperty("hasChild"))
var BUGNUMBER = 898359;
var summary = 'TypedObjects reference type trace';
var actual = '';
var expect = '';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var Any = TypedObject.Any;
var Object = TypedObject.Object;
var string = TypedObject.string;

function TestStructFields(RefType) {
  var rabbit = {};
  var S1 = new StructType({f: RefType});
  var s1 = new S1({f: rabbit});
  assertEq(hasChild(s1, rabbit), true);
  s1.f = null;
  assertEq(hasChild(s1, rabbit), false);
}

function TestArrayElements(RefType) {
  var rabbit = {};
  var S1 = new ArrayType(RefType, 1);
  var s1 = new S1([rabbit]);
  assertEq(hasChild(s1, rabbit), true);
  s1[0] = null;
  assertEq(hasChild(s1, rabbit), false);
}

function TestStructInArray(RefType) {
  var rabbit = {};
  var S2 = new StructType({f: RefType, g: RefType});
  var S1 = new ArrayType(S2, 1);
  var s1 = new S1([{f: rabbit, g: {}}]);
  assertEq(hasChild(s1, rabbit), true);
  s1[0].f = null;
  assertEq(hasChild(s1, rabbit), false);
}

function TestStringInStruct() {
  // Rather subtle hair-pullingly maddening testing phenomena: If you
  // just use a constant string here, it's always reachable via the
  // atoms table. Same is true of "Hello" + "1" (an earlier
  // attempt) due to parser constant folding. So we have to make a
  // rabbit that's not constant foldable. But don't just use
  // Math.random(), since small integers are atoms already.
  var rabbit = "Hello" + Math.random();
  var S1 = new StructType({f: string});
  var s1 = new S1({f: rabbit});
  assertEq(hasChild(s1, rabbit), true);
  s1.f = "World";
  assertEq(hasChild(s1, rabbit), false);
}

function runTests()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  TestStructFields(Object);
  TestStructFields(Any);

  TestArrayElements(Object);
  TestArrayElements(Any);

  TestStructInArray(Object);
  TestStructInArray(Any);

  TestStringInStruct();

  reportCompare(true, true, "TypedObjects trace tests");
}

runTests();
