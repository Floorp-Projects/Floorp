actual = '';
expected = 'true,';

function isNotEmpty(args, i) {
  var o = args[i];
  if (!(o && o.length)) {
    return false;
  }
  return true;
};

function f(obj) {
  for (var i = 0; i < arguments.length; i++) {
    if (!isNotEmpty(arguments, i))
      return false;
  }
  return true;
}

appendToActual(f([1], [1], [1], "asdf", [1]));


assertEq(actual, expected)
