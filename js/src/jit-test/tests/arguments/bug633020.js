var N = 10;
function f(b) {
    var a = [];
    for (var i = 0; i < N; i++)
        a[i] = {};
    a[N-1] = arguments;
    for (var i = 0; i < N; i++)
        a[i][0] = i;
    assertEq(b, N - 1);
}
f(null);
