// Test transpiling of LoadArgumentsObjectLengthResult and cover all possible bailout conditions.

function blackhole() {
  // Direct eval prevents any compile-time optimisations.
  eval("");
}

function testLengthAccess() {
  blackhole(arguments); // Create an arguments object.

  for (var i = 0; i < 50; ++i) {
    assertEq(arguments.length, 1);
  }
}
for (var i = 0; i < 20; ++i) testLengthAccess(1);

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

    var expected = 0 + (i < 25);
    assertEq(arguments.length, expected);
  }
}
for (var i = 0; i < 20; ++i) testBailoutLengthReified(1);


function deleteLengthIf(args, cond) {
  with ({}) ; // Don't Warp compile to avoid cold code bailouts.
  if (cond) {
    delete args.length;
  }
}

function testBailoutLengthDeleted() {
  blackhole(arguments); // Create an arguments object.

  // Load expected values from an array to avoid possible cold code bailouts.
  var values = [1, undefined];

  for (var i = 0; i < 50; ++i) {
    deleteLengthIf(arguments, i === 25);

    var expected = values[0 + (i >= 25)];
    assertEq(arguments.length, expected);
  }
}
for (var i = 0; i < 20; ++i) testBailoutLengthDeleted(1);
