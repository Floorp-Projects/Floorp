// Test transpiling of LoadArgumentsObjectArgResult and cover all possible bailout conditions.

function blackhole() {
  // Direct eval prevents any compile-time optimisations.
  eval("");
}

function testConstantArgAccess() {
  blackhole(arguments); // Create an arguments object.

  for (var i = 0; i < 50; ++i) {
    assertEq(arguments[0], 1);
  }
}
for (var i = 0; i < 20; ++i) testConstantArgAccess(1);

function testDynamicArgAccess() {
  blackhole(arguments); // Create an arguments object.

  for (var i = 0; i < 50; ++i) {
    assertEq(arguments[i & 1], 1 + (i & 1));
  }
}
for (var i = 0; i < 20; ++i) testDynamicArgAccess(1, 2);

function markElementOveriddenIf(args, cond, value) {
  with ({}) ; // Don't Warp compile to avoid cold code bailouts.
  if (cond) {
    Object.defineProperty(args, 0, {value});
  }
}

function testBailoutElementReified() {
  blackhole(arguments); // Create an arguments object.

  for (var i = 0; i < 50; ++i) {
    markElementOveriddenIf(arguments, i === 25, 2);

    var expected = 1 + (i >= 25);
    assertEq(arguments[0], expected);
  }
}
for (var i = 0; i < 20; ++i) testBailoutElementReified(1);

function markLengthOveriddenIf(args, cond, value) {
  with ({}) ; // Don't Warp compile to avoid cold code bailouts.
  if (cond) {
    args.length = value;
  }
}

function testBailoutLengthReified() {
  blackhole(arguments); // Create an arguments object.

  for (var i = 0; i < 50; ++i) {
    markLengthOveriddenIf(arguments, i === 25, 0);

    assertEq(arguments[0], 1);
  }
}
for (var i = 0; i < 20; ++i) testBailoutLengthReified(1);

function deleteElementIf(args, cond) {
  with ({}) ; // Don't Warp compile to avoid cold code bailouts.
  if (cond) {
    delete args[0];
  }
}

function testBailoutElementDeleted() {
  blackhole(arguments); // Create an arguments object.

  // Load expected values from an array to avoid possible cold code bailouts.
  var values = [1, undefined];

  for (var i = 0; i < 50; ++i) {
    deleteElementIf(arguments, i === 25);

    var expected = values[0 + (i >= 25)];
    assertEq(arguments[0], expected);
  }
}
for (var i = 0; i < 20; ++i) testBailoutElementDeleted(1);

function testBailoutOutOfBounds() {
  blackhole(arguments); // Create an arguments object.

  // Load expected values from an array to avoid possible cold code bailouts.
  var values = [1, undefined];

  for (var i = 0; i < 50; ++i) {
    var index = 0 + (i >= 25);
    var expected = values[index];
    assertEq(arguments[index], expected);
  }
}
for (var i = 0; i < 20; ++i) testBailoutOutOfBounds(1);

function testBailoutArgForwarded(arg1, arg2) {
  blackhole(arguments); // Create an arguments object.
  blackhole(() => arg2); // Ensure |arg2| is marked as "forwarded".

  for (var i = 0; i < 50; ++i) {
    var index = 0 + (i >= 25);
    var expected = 1 + (i >= 25);
    assertEq(arguments[index], expected);
  }
}
for (var i = 0; i < 20; ++i) testBailoutArgForwarded(1, 2);
