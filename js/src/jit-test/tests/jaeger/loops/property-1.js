
function foo(x, y) {
  var res = 0;
  for (var i = 0; i < 10; i++) {
    res += x.f + y[i];
  }
  return res;
}

var x = {f:0};
var y = Array(10);
for (var i = 0; i < 10; i++) {
  if (i == 5)
    Object.defineProperty(Object.prototype, 5, {get: function() { x.f = 10; return 5}});
  else
    y[i] = i;
}

assertEq(foo(x, y), 85);
