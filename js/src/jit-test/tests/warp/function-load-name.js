// Test transpiling of LoadFunctionNameResult and cover possible bailout conditions.

function empty() {}

// Note: Typically won't use LoadFunctionNameResult, because the "name"
// property will be resolved on the first access.
function testGlobalFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(empty.name, "empty");
  }
}
testGlobalFunction();

// Note: Typically won't use LoadFunctionNameResult, because the "name"
// property will be resolved on the first access.
function testInnerFunction() {
  function f() {}
  for (var i = 0; i < 200; ++i) {
    assertEq(f.name, "f");
  }
}
testInnerFunction();

function testPerLoopFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(function f(){}.name, "f");
  }
}
testPerLoopFunction();

// Note: Typically won't use LoadFunctionNameResult, because the "name"
// property will be resolved on the first access.
function testNativeFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(Math.max.name, "max");
  }
}
testNativeFunction();

// Note: Typically won't use LoadFunctionNameResult, because the "name"
// property will be resolved on the first access.
function testSelfHostedFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(Array.prototype.forEach.name, "forEach");
  }
}
testSelfHostedFunction();

// Bailout when the name property is resolved.
function testBailoutResolvedName() {
  function f1() {}

  // Ensure the name property of |f1| is resolved.
  assertEq(f1.name, "f1");

  var names = ["f", "f1"];

  for (var i = 0; i < 10; ++i) {
    var name = names[0 + (i >= 5)];

    for (var j = 0; j < 100; ++j) {
      var values = [function f(){}, f1];
      var value = values[0 + (i >= 5)];

      assertEq(value.name, name);
    }
  }
}
testBailoutResolvedName();

// Bailout when the HAS_BOUND_FUNCTION_NAME_PREFIX isn't set.
function testBailoutBoundName() {
  function f1() {}
  function f2() {}

  var bound = f1.bind();

  // Ensure the name property of |bound| is resolved. That way new functions
  // created through |bound().bind()| will have the HAS_BOUND_FUNCTION_NAME_PREFIX
  // flag set.
  assertEq(bound.name, "bound f1");

  // |bound1| and |bound2| have the same shape, but different function flags.
  var bound1 = bound.bind(); // HAS_BOUND_FUNCTION_NAME_PREFIX
  var bound2 = f2.bind(); // ! HAS_BOUND_FUNCTION_NAME_PREFIX

  var values = [bound1, bound2];
  var names = ["bound bound bound f1", "bound bound f2"];

  for (var i = 0; i < 10; ++i) {
    var value = values[0 + (i >= 5)];
    var name = names[0 + (i >= 5)];

    for (var j = 0; j < 100; ++j) {
      var f = value.bind();
      assertEq(f.name, name);
    }
  }
}
testBailoutBoundName();
