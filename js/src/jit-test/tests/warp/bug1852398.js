// |jit-test| --no-threads

function inner(obj, f) {
  return obj.x + f();
}

function middle(obj, f) {
  return inner(obj, f);
}

function outer(obj, f) {
  return middle(obj, f);
}

var fs = [() => 1, () => 2];

with ({}) {}
for (var i = 0; i < 1500; i++) {
  var obj = {x: 1};
  obj["y" + i % 2] = 2;
  outer(obj, fs[i % 2]);
}
for (var i = 0; i < 1500; i++) {
  var obj = {x: 1};
  obj["y" + (3 + (i % 10))] = 2;
  outer(obj, fs[i % 2]);
}
