var target = undefined;
function throwOn(x) {
  with ({}) {}
  if (x === target) {
    throw x;
  }
}

// Test nested try-finally
function foo() {
  let result = 0;
  try {
    throwOn(1);
    result += 1;
    try {
      throwOn(2);
      result += 2;
    } finally {
      result += 3;
    }
  } finally {
    return result;
  }
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  assertEq(foo(), 6);
}

target = 1;
assertEq(foo(), 0);

target = 2;
assertEq(foo(), 4);
