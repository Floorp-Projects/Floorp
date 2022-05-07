function g(array) {
  // 1. Absent properties return |undefined| from CacheIR.
  // 2. Tests on |undefined| are changed to |false| in CacheIR.
  //
  // When Warp compiling the CacheIR ops, the first test will then happen on
  // a boolean, whereas the phi still sees the original undefined value.
  if (array.does_not_exist || array.does_not_exist_too || array.slice) {
    return 1;
  }
  return 0;
}

function f() {
  var array = [];
  var r = 0;
  for (let i = 0; i < 100; ++i) {
    r += g(array);
  }
  assertEq(r, 100);
}

for (let i = 0; i < 2; ++i) f();