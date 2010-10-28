actual = '';
expected = '3,6,9,12,15,18,';

function slice(a, b)
{
  //return { x: a + ':' + b };
  return b;
}

function f(index)
{
  var length = 20;

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

f(0);


assertEq(actual, expected)
