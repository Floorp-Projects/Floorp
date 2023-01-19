function id(x) { return x; }

function foo(obj) {
  for (var key in obj) {
    assertEq(id(obj[key]), obj[key]);
  }
}

var arr = [];
for (var i = 0; i < 8; i++) {
  var obj = {["x" + i]: 1};
  arr.push(obj);
}

with ({}) {}
for (var i = 0; i < 1000; i++) {
  let obj = arr[i % arr.length];
  foo(obj);
}
