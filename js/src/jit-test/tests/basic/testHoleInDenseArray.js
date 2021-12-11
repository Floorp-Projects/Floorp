var s;

function f(i) {
    if (i > 4) /* side exit when arr[i] changes from bool to undefined (via a hole) */
        assertEq(s, undefined);
    else
        assertEq(s, false);
    return 1;
}

/* trailing 'true' ensures array has capacity >= 10 */
var arr = [ false, false, false, false, false, , , , , , true ];

for (var i = 0; i < 10; ++i) {
    (s = arr[i]) + f(i);
}
