
function foo(x, n, y) {
  var q = 0;
  for (var j = 0; j < n; j++) {
    if (x[j] < y)
      q++;
  }
  assertEq(q, 1);
}

var x = [1,2,3,4,5];
var y = { valueOf: function() { x.length = 0; return 6; } };

var a = foo(x, 5, y);
