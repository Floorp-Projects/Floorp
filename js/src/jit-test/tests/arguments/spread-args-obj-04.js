var result;

function g(a, b) {
    with ({}) {}
    result = a + b;
}

function escape() { with({}) {} }

function f() {
    escape(arguments);
    for (var i = 0; i < 50; ++i) {
        g(...arguments);
    }
}

f(1, 2);
assertEq(result, 3);

f("");
assertEq(result, "undefined");
