class Iterator {
  val = 0;
  next() {
    return { value: this.val++, done: false }
  }
  return() { return { value: undefined, done: true }}
}

var arr = [];
for (var i = 0; i < 10; i++) {
  class SubIter extends Iterator {}
  var iterable = {
    [Symbol.iterator]() { return new SubIter(); }
  }
  arr.push(iterable);
}

function foo() {
  for (var x of arr[i % arr.length]) {
    if (x > 1) { return; }
  }
}

with ({}) {}
for (var i = 0; i < 100; i++) { foo(); }
