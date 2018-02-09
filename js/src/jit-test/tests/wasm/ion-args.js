let { exports } = wasmEvalText(`(module
    (func (export "i32") (result i32) (param i32)
     get_local 0
    )

    (func (export "f32") (result f32) (param f32)
     get_local 0
    )

    (func (export "f64") (result f64) (param f64)
     get_local 0
    )
)`);

const options = getJitCompilerOptions();
const jitThreshold = options['ion.warmup.trigger'] * 2;

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
