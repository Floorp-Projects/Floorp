actual = '';
expected = '0,0,0,0,0,88,88,88,88,88,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,';

function h(k) {
  for (var i = 0; i < 5; ++i) {
    var a = arguments;
    appendToActual(a[k]);
  }
}

h(0);
h(2, 99, 88, 77);
h(-5);
h(46);
h('adf');


assertEq(actual, expected)
