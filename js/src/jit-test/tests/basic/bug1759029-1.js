// |jit-test| --fast-warmup; --no-threads

var arr = [];
arr[0] = 1;
arr[NaN] = 0;

function foo() {
  for (let i = 0; i < 7; i++) {
    const a = i % i;
    counter += a >>> a;

    try {
      throw 3;
    } catch {
      counter += arr[a];
    }
  }
  for (let i = 0; i < 100; i++) { }
}

let counter = 0;
for (var i = 0; i < 10; i++) {
  foo();
}

assertEq(counter, 60);
