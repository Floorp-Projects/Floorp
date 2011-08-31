
/* Recompilation that requires patching the same native stub multiple times on one stack. */

var first = 0;
var second = 0;

function eacher(f, vfirst, vsecond) {
  var a = [0];
  a.forEach(f);
  assertEq(first, vfirst);
  assertEq(second, vsecond);
}

function one() {
  eacher(two, 'one', 'two');
}

function two() {
  eval("first = 'one';");
  eval("second = 'two';");
}

eacher(function () {}, 0, 0);
eacher(function () {}, 0, 0);
eacher(one, 'one', 'two');
