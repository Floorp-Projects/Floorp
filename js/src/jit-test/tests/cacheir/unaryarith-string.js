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

// Unary - operator with transition between string representations of int32 and double.
var fun_neg_int32_double = (x) => { return -x; }
var fun_neg_double_int32 = (x) => { return -x; }

// Unary ~ operator using string representations of either int32 or double.
var fun_bitnot_int32 = (x) => { return ~x; }
var fun_bitnot_double = (x) => { return ~x; }

// Unary ++ operator using string representations of either int32 or double.
var fun_inc_int32 = (x) => { return ++x; }
var fun_inc_double = (x) => { return ++x; }

// Unary -- operator using string representations of either int32 or double.
var fun_dec_int32 = (x) => { return --x; }
var fun_dec_double = (x) => { return --x; }

// Unary + operator using string representations of either int32 or double.
var fun_pos_int32 = (x) => { return +x; }
var fun_pos_double = (x) => { return +x; }

// JSOp::ToNumeric using string representations of either int32 or double.
var fun_tonumeric_int32 = (x) => { return x++; }
var fun_tonumeric_double = (x) => { return x++; }

warmup(fun_neg_int32_double, ["1", "2", "-3"], [-1, -2, 3]);
warmup(fun_neg_double_int32, ["0"], [-0]);

warmup(fun_neg_double_int32, ["3", "4", "-5"], [-3, -4, 5]);
warmup(fun_neg_int32_double, ["1.2", "1.4"], [-1.2, -1.4]);

warmup(fun_bitnot_int32, ["-1", "0"], [0, -1]);
warmup(fun_bitnot_double, ["-1.0", "0.0", "1.2", "3"], [0, -1, -2, -4]);

warmup(fun_inc_int32, ["-1", "0"], [0, 1]);
warmup(fun_inc_double, ["-1.0", "0.0", "1.2", "3"], [0, 1, 2.2, 4]);

warmup(fun_dec_int32, ["-1", "0"], [-2, -1]);
warmup(fun_dec_double, ["-1.0", "0.0", "1.5", "3"], [-2, -1, 0.5, 2]);

warmup(fun_pos_int32, ["-1", "0"], [-1, 0]);
warmup(fun_pos_double, ["-1.0", "0.0", "1.2", "3"], [-1.0, 0.0, 1.2, 3]);

warmup(fun_tonumeric_int32, ["-1", "0"], [-1, 0]);
warmup(fun_tonumeric_double, ["-1.0", "0.0", "1.2", "3"], [-1.0, 0.0, 1.2, 3]);
