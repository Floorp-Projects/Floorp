actual = '';
expected = '3,6,9,12,15,18,';

function slice(a, b)
{
  //return { x: a + ':' + b };
  return b;
}

function f()
{
  var length = 20;
  var index = 0;

  function get3() {
    //appendToActual("get3 " + index);
    if (length - index < 3)
      return null;
    return slice(index, index += 3);
  }

  var bytes = null;
  while (bytes = get3()) {
    appendToActual(bytes);
  }
}

f();


assertEq(actual, expected)
