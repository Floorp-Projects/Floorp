actual = '';
expected = '77,77,';

var o = {
 x: 11,
 y: 13
};

function g() {
  with (o) {
    var x = 77;
    for (var i = 0; i < 5; ++i) {
      var f = function() { appendToActual(x); }
    }
    f();
  }

  f();
}

g();


assertEq(actual, expected)
