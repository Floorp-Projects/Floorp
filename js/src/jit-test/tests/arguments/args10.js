actual = '';
expected = 'function h(x, y) { return arguments; },2,4,8,';

function h(x, y) { return arguments; }

var p;
for (var i = 0; i < 5; ++i) {
  p = h(i, i*2);
}
appendToActual(p.callee);
appendToActual(p.length);
appendToActual(p[0]);
appendToActual(p[1]);


assertEq(actual, expected)
