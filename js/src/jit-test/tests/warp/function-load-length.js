// Test transpiling of LoadFunctionLengthResult and cover possible bailout conditions.

function empty() {}

// Note: Typically won't use LoadFunctionLengthResult, because the "length"
// property will be resolved on the first access.
function testGlobalFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(empty.length, 0);
  }
}
testGlobalFunction();

// Note: Typically won't use LoadFunctionLengthResult, because the "length"
// property will be resolved on the first access.
function testInnerFunction() {
  function f() {}
  for (var i = 0; i < 200; ++i) {
    assertEq(f.length, 0);
  }
}
testInnerFunction();

function testPerLoopFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(function(){}.length, 0);
  }
}
testPerLoopFunction();

// Note: Typically won't use LoadFunctionLengthResult, because the "length"
// property will be resolved on the first access.
function testNativeFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(Math.max.length, 2);
  }
}
testNativeFunction();

// Note: Typically won't use LoadFunctionLengthResult, because the "length"
// property will be resolved on the first access.
function testSelfHostedFunction() {
  for (var i = 0; i < 200; ++i) {
    assertEq(Array.prototype.forEach.length, 1);
  }
}
testSelfHostedFunction();

// Bailout when the length doesn't fit into int32.
function testBailoutLength() {
  var values = [0, 0x80000000];
  var bound = empty.bind();

  for (var i = 0; i < 10; ++i) {
    var value = values[0 + (i >= 5)];

    // Define on each iteration to get the same shape.
    Object.defineProperty(bound, "length", {value});

    for (var j = 0; j < 100; ++j) {
      var f = bound.bind();
      assertEq(f.length, value);
    }
  }
}
testBailoutLength();

// Bailout when trying to read "length" from a property with a lazy script.
function testBailoutLazyFunction() {
  for (var i = 0; i < 200; ++i) {
    var values = [function(){}, function(a){}];
    var index = 0 + (i >= 100);
    assertEq(values[index].length, index);
  }
}
testBailoutLazyFunction();

// Bailout when trying to read "length" from a property with a lazy self-hosted script.
function testBailoutLazySelfHostedFunction() {
  for (var i = 0; i < 200; ++i) {
    var values = [function(){}, Array.prototype.map];
    var index = 0 + (i >= 100);
    assertEq(values[index].length, index);
  }
}
testBailoutLazySelfHostedFunction();
