
var res = 0;

function foo(x, n, y) {
  for (var j = 0; j < n; j++) {
    x[j];
    y.f;
  }
}

var x = [1,2,3,4,5];
var y = {};
Object.defineProperty(y, 'f', {get:function() { res++; x.length = 2; }});

var a = foo(x, 5, y);

assertEq(res, 5);
