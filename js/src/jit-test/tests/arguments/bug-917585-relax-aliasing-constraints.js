
function foo(a, b) {
    blah(function (x) { a = x; }, b);
    return arguments[0];
}

function blah(f, b) {
    f(b);
}

function main() {
    for (var i = 0; i < 1500; i++) {
        var x = foo(i, i*2);
        assertEq(x, i*2);
    }
}

main();
