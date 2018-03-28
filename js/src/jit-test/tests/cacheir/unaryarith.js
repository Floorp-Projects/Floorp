setJitCompilerOption('ion.forceinlineCaches', 1);

function warmup(fun, input_array, output_array) {
    assertEq(output_array.length, input_array.length);
    for (var index = 0; index < input_array.length; index++) {
        input = input_array[index];
        output = output_array[index];
        for (var i = 0; i < 30; i++) {
            var y = fun(input);
            assertEq(y, output)
        }
    }
}

var fun1 = (x) => { return -x; }
var fun2 = (x) => { return -x; }

var fun3 = (x) => { return ~x; }
var fun4 = (x) => { return ~x; }

warmup(fun1, [1, 2], [-1, -2]);
warmup(fun2, [0], [-0]);

warmup(fun2, [3, 4], [-3, -4]);
warmup(fun1, [1.2, 1.4], [-1.2, -1.4]);

warmup(fun3, [-1, 0], [0, -1]);
warmup(fun4, [-1.0, 0.0, 1.2, 3], [0, -1, -2, -4]);

