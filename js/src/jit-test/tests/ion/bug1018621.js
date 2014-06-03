function strictSome(k) {
  "use strict";
  for (var i = 0; i < args.length; i++)
    assertEq(arguments[i], args[i], "wrong argument " + i);
}
args = [8, 6, 7, NaN, undefined, 0.3];
strictSome.call(NaN, 8, 6, 7, NaN, undefined, 0.3);
