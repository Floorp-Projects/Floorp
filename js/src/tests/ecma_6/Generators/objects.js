// This file was written by Andy Wingo <wingo@igalia.com> and originally
// contributed to V8 as generators-objects.js, available here:
//
// http://code.google.com/p/v8/source/browse/branches/bleeding_edge/test/mjsunit/harmony/generators-objects.js

// Test aspects of the generator runtime.

// Test the properties and prototype of a generator object.
function TestGeneratorObject() {
  function* g() { yield 1; }

  var iter = g();
  assertEq(Object.getPrototypeOf(iter), g.prototype);
  assertTrue(iter instanceof g);
  assertEq(String(iter), "[object Generator]");
  assertDeepEq(Object.getOwnPropertyNames(iter), []);
  assertNotEq(g(), iter);

  // g() is the same as new g().
  iter = new g();
  assertEq(Object.getPrototypeOf(iter), g.prototype);
  assertTrue(iter instanceof g);
  assertEq(String(iter), "[object Generator]");
  assertDeepEq(Object.getOwnPropertyNames(iter), []);
  assertNotEq(new g(), iter);
}
TestGeneratorObject();


// Test the methods of generator objects.
function TestGeneratorObjectMethods() {
  function* g() { yield 1; }
  var iter = g();

  function TestNonGenerator(non_generator) {
    assertThrowsInstanceOf(function() { iter.next.call(non_generator); }, TypeError);
    assertThrowsInstanceOf(function() { iter.next.call(non_generator, 1); }, TypeError);
    assertThrowsInstanceOf(function() { iter.throw.call(non_generator, 1); }, TypeError);
    assertThrowsInstanceOf(function() { iter.close.call(non_generator); }, TypeError);
  }

  TestNonGenerator(1);
  TestNonGenerator({});
  TestNonGenerator(function(){});
  TestNonGenerator(g);
  TestNonGenerator(g.prototype);
}
TestGeneratorObjectMethods();


if (typeof reportCompare == "function")
    reportCompare(true, true);
