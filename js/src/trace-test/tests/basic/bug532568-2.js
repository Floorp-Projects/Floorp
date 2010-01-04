var before = '';
var after = '';

function g(b) {
  for (var i = 0; i < 10; ++i) {
  }
}

function f(xa_arg) {
  var xa = xa_arg;
  for (var i = 0; i < 5; ++i) {
    var j = i + xa[i];
    before += j + ',';
    g(); 
    after += j + ',';
  }
}

f([ 0, 1, 2, 3, 4 ]);
assertEq(before, after);
