function map_test(t, cases)
{
  for (var i = 0; i < cases.length; i++) {
    function c() { return t(cases[i].input); }
    var expected = cases[i].expected;
    assertEq(c(), expected);
  }
}

function lsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x1 << n;
  return r;
}
map_test (lsh_inner,
          [{input: 15, expected: 32768},
           {input: 55, expected: 8388608},
           {input: 1,  expected: 2},
           {input: 0,  expected: 1}]);

function rsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x11010101 >> n;
  return r;
}
map_test (rsh_inner,
          [{input: 8,  expected: 1114369},
           {input: 5,  expected: 8914952},
           {input: 35, expected: 35659808},
           {input: -1, expected: 0}]);

function ursh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = -55 >>> n;
  return r;
}
map_test (ursh_inner,
          [{input: 8,  expected: 16777215},
           {input: 33, expected: 2147483620},
           {input: 0,  expected: 4294967241},
           {input: 1,  expected: 2147483620}]);

function doMath_inner(cos)
{
    var s = 0;
    var sin = Math.sin;
    for (var i = 0; i < 200; i++)
        s = -Math.pow(sin(i) + cos(i * 0.75), 4);
    return s;
}
function doMath() {
  return doMath_inner(Math.cos);
}
assertEq(doMath(), -0.5405549555611059);
