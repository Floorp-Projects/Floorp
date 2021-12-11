
/* Recompilation that requires patching the same native stub multiple times. */

var first = 0;
var second = 0;

function foreachweird(a, f, vfirst, vsecond)
{
  a.forEach(f);
  assertEq(first, vfirst);
  assertEq(second, vsecond);
}

function weird() {
  eval("first = 'one';");
  eval("second = 'two';");
}

foreachweird([0], function() {}, 0, 0);
foreachweird([0], function() {}, 0, 0);
foreachweird([0], weird, 'one', 'two');
