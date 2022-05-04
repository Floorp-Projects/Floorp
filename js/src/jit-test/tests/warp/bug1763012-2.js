// |jit-test| --fast-warmup; --no-threads

function f() {
  let counter = 0;
  for (var i = 0; i < 8; i++) {
    let x = 0 % i;
    for (var j = 0; j < 6; j++) {
      if (x === x) {
        if (j > 6) {} else {}
      } else {
        counter += 1;
      }
    }
  }
  return counter;
}

f();
assertEq(f(), 6);
