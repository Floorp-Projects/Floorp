var do_abs = function(x) {
    return Math.abs(x);
}
var i;
for (i=0;i<50;i++)
    do_abs(-1.5);
assertEq(do_abs(-2.5), 2.5);
