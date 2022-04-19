function* a() {
  try {
    yield 1;
  } finally {
    for (c = 0; c < 100; c++);
  }
}

var b = a();
b.next();
let result = b.return(42);
assertEq(result.value, 42);
assertEq(result.done, true);
