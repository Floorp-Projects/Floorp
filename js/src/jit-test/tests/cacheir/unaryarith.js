function warmup(fun, input, output) {
    for (var i = 0; i < 30; i++) {
        var y = fun(input);
        assertEq(y, output)
    }
}

var fun1 = (x) => { return -x; }
var fun2 = (x) => { return -x; }

var fun3 = (x) => { return ~x; }
var fun4 = (x) => { return ~x; }

warmup(fun1, 1, -1);
warmup(fun1, 0, -0);

warmup(fun2, 3, -3);
warmup(fun2, 1.2, -1.2);

warmup(fun3, -1, 0);
warmup(fun4, 1.2, -2);
warmup(fun4, 3, -4)

