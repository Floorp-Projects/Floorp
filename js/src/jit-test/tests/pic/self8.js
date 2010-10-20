// getprop, self, 8 shapes

var expected = "0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,";
var actual = '';

function letter(i) {
  return String.fromCharCode(97 + i);
}

function f() {
  // Build 8 objects with different shapes and x in different slots.
  var oa = [];
  for (var i = 0; i < 8; ++i) {
    var o = {};
    for (var j = 0; j < 8; ++j) {
      if (j != i) {
	o[letter(j)] = 1000 + i * 10 + j;
      } else {
	o.x = i;
      }
    }
    oa[i] = o;
  }

  for (var i = 0; i < 24; ++i) {
    actual += oa[i%8].x + ',';
  }
}

f();

assertEq(actual, expected);
