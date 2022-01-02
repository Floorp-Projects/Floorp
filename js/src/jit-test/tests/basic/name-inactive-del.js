function mp(g) {
  var ans = '';
  for (var i = 0; i < 5; ++i) {
    ans += g();
  }

  return ans;
}

function f() {
  var k = 5;

  function g() {
    return k;
  }

  ans = '';

  k = 6;
  ans += mp(g);

  delete k;
  ans += mp(g);

  return ans;
}

assertEq(f(), '6666666666');
