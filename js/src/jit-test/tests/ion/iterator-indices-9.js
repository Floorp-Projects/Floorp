// Tests SetPropertyCache sets to object.
function test(obj) {
  let index = 0;
  for (var s in obj) {
    if (s.startsWith("test")) {
      obj[s] = index;
    }
    index++;
  }
  index = 0;
  for (var s in obj) {
    if (s.startsWith("test")) {
      assertEq(obj[s], index);
    }
    index++;
  }
}

var arr = [];
for (var i = 0; i < 2; i++) {
  var obj = {};
  for (var j = 0; j < i * 20; j++) {
    obj["x_" + i + "_" + j] = 1;
  }
  obj.testKey = 1;
  arr.push(obj);
}

with ({}) {}
for (var i = 0; i < 2000; i++) {
  var idx = i % arr.length;
  test(arr[idx]);
}

