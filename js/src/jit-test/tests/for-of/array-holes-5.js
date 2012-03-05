// for-of does not skip trailing holes; the value is undefined.

var log = "";
for (var x of [1, 2, 3,,])
    log += x;
assertEq(log, "123undefined");
