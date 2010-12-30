function g() {
    return "global";
}

function q(fun) {
    return fun();
}

function f(x) {
    if (x) {
        function g() {
            return "local";
        }
        var ans = q(function() {
            return g();
        });
    }
    g = null;
    return ans;
}

assertEq(f(true), "local");
