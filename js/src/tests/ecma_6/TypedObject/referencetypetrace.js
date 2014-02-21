// |reftest| skip-if(!this.hasOwnProperty("TypedObject")||!this.hasOwnProperty("countHeap"))
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

function assertCanReach(from, target, times) {
  times = times || 1;
  var count1 = countHeap(from, "specific", target);
  print("canReach:", count1, times, from.toSource(), target.toSource());
  assertEq(count1, times);
}

function assertCannotReach(from, target) {
  var count1 = countHeap(from, "specific", target);
  print("cannotReach:", count1, from.toSource(), target.toSource());
  assertEq(count1, 0);
}

function TestStructFields(RefType) {
  var rabbit = {};
  var S1 = new StructType({f: RefType});
  var s1 = new S1({f: rabbit});
  assertCanReach(s1, rabbit);
  s1.f = null;
  assertCannotReach(s1, rabbit);
}

function TestArrayElements(RefType) {
  var rabbit = {};
  var S1 = new ArrayType(RefType).dimension(1);
  var s1 = new S1([rabbit]);
  assertCanReach(s1, rabbit);
  s1[0] = null;
  assertCannotReach(s1, rabbit);
}

function TestUnsizedArrayElements(RefType) {
  var rabbit = {};
  var S1 = new ArrayType(RefType);
  var s1 = new S1([rabbit]);
  assertCanReach(s1, rabbit);
  s1[0] = null;
  assertCannotReach(s1, rabbit);
}

function TestStructInArray(RefType) {
  var rabbit = {};
  var S2 = new StructType({f: RefType, g: RefType});
  var S1 = new ArrayType(S2).dimension(1);
  var s1 = new S1([{f: rabbit, g: {}}]);
  assertCanReach(s1, rabbit);
  s1[0].f = null;
  assertCannotReach(s1, rabbit);
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
  assertCanReach(s1, rabbit);
  s1.f = "World";
  assertCannotReach(s1, rabbit);
}

function runTests()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  TestStructFields(Object);
  TestStructFields(Any);

  TestArrayElements(Object);
  TestArrayElements(Any);

  TestUnsizedArrayElements(Object);
  TestUnsizedArrayElements(Any);

  TestStructInArray(Object);
  TestStructInArray(Any);

  TestStringInStruct();

  reportCompare(true, true, "TypedObjects trace tests");
}

runTests();
