var verified = false;
function f(a) {
    if (a < 10000)
        return 5;
    assertEq(g_fwd.caller.arguments.length, 0);
    assertEq(h_fwd.caller.arguments.length, 0);
    verified = true;
    return 6;
}

function g_fwd(x) {
    with({}) {};
    return f(x);
}
function g(a) {
    var x = a;
    function inline() {
        return g_fwd(x);
    }
    return inline();
}

function h_fwd(x) {
    with({}) {};
    return g(x);
}
function h(a) {
    var x = a;
    function inline() {
        return h_fwd(x);
    }
    return inline();
}

var i = 0;
while (!verified) {
    h(i++);
}
