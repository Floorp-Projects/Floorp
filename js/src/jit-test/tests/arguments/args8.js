actual = '';
expected = '[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],';

function h() {
  return arguments;
}

for (var i = 0; i < 5; ++i) {
  var p = h(i, i*2);
  appendToActual(p);
}


assertEq(actual, expected)
