function test(obj, expected) {
  var actual = 0;
  for (var s in obj) {
    actual += obj[s];
  }
  assertEq(actual, expected);
}

var arr = [];
var elem_obj = [];
for (var i = 0; i < 20; i++) {
  var obj = {};
  for (var j = 0; j < i; j++) {
    obj["x_" + i + "_" + j] = 1;
  }
  arr.push(obj);
  elem_obj.push(1);
}
arr.push(elem_obj);

with ({}) {}
for (var i = 0; i < 2000; i++) {
  var idx = i % arr.length;
  test(arr[idx], idx);
}
