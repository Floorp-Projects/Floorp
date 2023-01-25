var arr = [];
for (var i = 0; i < 10; i++) {
  arr.push({y: 1, x: 2, ["z" + i]: 3});
}

function bar(x) { with ({}) {} }

function foo(obj) {
  for (var key in obj) {
    bar(obj[key]);
  }
}

with ({}) {}
for (var i = 0; i < 2000; i++) {
  foo(arr[i % arr.length]);
}
