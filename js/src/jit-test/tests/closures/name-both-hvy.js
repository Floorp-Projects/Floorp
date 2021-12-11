actual = '';
expected = '';

// do not crash

function q() {
}

function f() {
  var j = 12;

  function g() {
    eval(""); // makes |g| heavyweight
    for (var i = 0; i < 3; ++i) {
      j;
    }
  }

  j = 13;
  q(g);  // escaping |g| makes |f| heavyweight
  g();
  j = 14;
}

f();


assertEq(actual, expected)
