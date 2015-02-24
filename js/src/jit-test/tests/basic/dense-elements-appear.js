function f() {
    var x = [1, 2, 3];
    var y = {};
    x.__proto__ = y;

    for (var i = 0; i < 200; i++) {
        if (i == 100)
            y[100000] = 15;
        else
            assertEq(typeof x[100000], i > 100 ? "number" : "undefined");
    }
}

function g() {
    var x = [1, 2, 3];
    var y = {};
    x.__proto__ = y;

    for (var i = 0; i < 200; i++) {
        if (i == 100)
            y[4] = 15;
        else
            assertEq(typeof x[4], i > 100 ? "number" : "undefined");
    }
}

f();
g();
