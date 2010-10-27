var g;

function h() {
    return arguments.length;
}

function f() {
    var args = arguments;
    g = function() { return h.apply(this, args); }
}

for (var i = 0; i < 10; ++i) {
    f();
}

assertEq(g(), 0);
