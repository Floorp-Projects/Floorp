// |jit-test| --fast-warmup

var sum = 0;

function foo(copy, shouldThrow) {
  switch (copy) {
   case 0:
    var x = 0;
    try {
      if (shouldThrow) { throw 0;}
      x = 1;
    } catch {
      x = "a";
    }
    // We create a specialized phi for x here, which bails out.
    for (var i = 0; i < 100; i++) {
      sum += x;
    }
    break;
   case 1:
    var y = 0;
    try {
      if (shouldThrow) { throw 0;}
      y = 1;
    } catch {
      y = "a";
    }
    // We do not create a specialized phi the second time.
    for (var i = 0; i < 100; i++) {
      sum += y;
    }
    break;
  }
}

with ({}) {}
for (var i = 0; i < 2; i++) {
  for (var j = 0; j < 50; j++) {
    foo(i, false);
  }
  foo(i, true);
}
