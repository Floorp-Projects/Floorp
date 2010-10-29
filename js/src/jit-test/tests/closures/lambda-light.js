actual = '';
expected = '10,';

function f(x) {
  let (x = 10) {
    for (var i = 0; i < 5; ++i) {
      var g = function () {
	appendToActual(x);
      };
    }
    g();
  }
}

f(1);


assertEq(actual, expected)
