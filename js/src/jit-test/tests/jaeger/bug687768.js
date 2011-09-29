
expected = '1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,';
function slice(a, b) {
  return expected;
}
function f() {
  var length = 4;
  var index = 0;
  function get3() {
    if (length - index < 3)
      return null;
    return slice(index, ++index);
  }
  var bytes = null;
  while (bytes = get3()) {  }
}
f();
