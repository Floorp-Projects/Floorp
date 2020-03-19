let { exports } = wasmEvalText(`(module
    (func (export "i32") (result i32) (param i32)
     local.get 0
    )

    (func (export "f32") (result f32) (param f32)
     local.get 0
    )

    (func (export "f64") (result f64) (param f64)
     local.get 0
    )

    (func (export "mixed_args") (result f64)
        (param i32) (param i32) (param i32) (param i32) (param i32) ;; 5 i32
        (param $f64 f64) ;; 1 f64
        (param i32)
     local.get $f64
    )
)`);

const options = getJitCompilerOptions();
const jitThreshold = options['ion.warmup.trigger'] * 2 + 2;

let coercions = {
    i32(x) { return x|0; },
    f32(x) { return Math.fround(x); },
    f64(x) { return +x; }
}

function call(func, coercion, arg) {
    let expected;
    try {
        expected = coercion(arg);
    } catch(e) {
        expected = e.message;
    }

    for (var i = jitThreshold; i --> 0;) {
        try {
            assertEq(func(arg), expected);
        } catch(e) {
            assertEq(e.message, expected);
        }
    }
}

// Test misc kinds of arguments.
(function() {
    const inputs = [
        42,
        3.5,
        -0,
        -Infinity,
        2**32,
        true,
        Symbol(),
        undefined,
        null,
        {},
        { valueOf() { return 13.37; } },
        "bonjour"
    ];

    for (let arg of inputs) {
        for (let func of ['i32', 'f32', 'f64']) {
            call(exports[func], coercions[func], arg);
        }
    }
})();

// Test mixup of float and int arguments.
(function() {
    for (let i = 0; i < 10; i++) {
        assertEq(exports.mixed_args(i, i+1, i+2, i+3, i+4, i+0.5, i+5), i+0.5);
    }
})();

// Test high number of arguments.
// All integers.
let {func} = wasmEvalText(`(module
    (func (export "func") (result i32)
        ${Array(32).join('(param i32)')}
        (param $last i32)
     local.get $last
    )
)`).exports;

(function() {
    for (let i = 0; i < 10; i++) {
        assertEq(func(i, i+1, i+2, i+3, i+4, i+5, i+6, i+7, i+8, i+9, i+10, i+11, i+12, i+13, i+14, i+15,
                      i+16, i+17, i+18, i+19, i+20, i+21, i+22, i+23, i+24, i+25, i+26, i+27, i+28, i+29, i+30, i+31
                 ), i+31);
    }
})();

// All floats.
func = wasmEvalText(`(module
    (func (export "func") (result i32)
        ${Array(32).join('(param f64)')}
        (param $last i32)
     local.get $last
    )
)`).exports.func;

(function() {
    for (let i = 0; i < 10; i++) {
        assertEq(func(i, i+1, i+2, i+3, i+4, i+5, i+6, i+7, i+8, i+9, i+10, i+11, i+12, i+13, i+14, i+15,
                      i+16, i+17, i+18, i+19, i+20, i+21, i+22, i+23, i+24, i+25, i+26, i+27, i+28, i+29, i+30, i+31
                 ), i+31);
    }
})();

// Mix em up! 1 i32, then 1 f32, then 1 f64, and again up to 32 args.
let params = [];
for (let i = 0; i < 32; i++) {
    params.push((i % 3 == 0) ? 'i32' :
                (i % 3 == 1) ? 'f32' :
                'f64'
               );
}

func = wasmEvalText(`(module
    (func (export "func") (result i32)
        ${Array(32).join('(param f64)')}
        (param $last i32)
     local.get $last
    )
)`).exports.func;

(function() {
    for (let i = 0; i < 10; i++) {
        assertEq(func(i, i+1, i+2, i+3, i+4, i+5, i+6, i+7, i+8, i+9, i+10, i+11, i+12, i+13, i+14, i+15,
                      i+16, i+17, i+18, i+19, i+20, i+21, i+22, i+23, i+24, i+25, i+26, i+27, i+28, i+29, i+30, i+31
                 ), i+31);
    }
})();
