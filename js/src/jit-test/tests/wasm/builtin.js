let values = [-Infinity, Infinity, NaN, 0, -0, -0.1, 0.1, 0.5, -1.6, 1.6, 13.37];

function unary(name) {
    print(name);

    let imports = {
        math: {
            func: Math[name],
        }
    };

    let f32 = x => Math.fround(Math[name](Math.fround(x)));
    let f64 = Math[name];

    let i = wasmEvalText(`(module
        (import $f32 "math" "func" (param f32) (result f32))
        (import $f64 "math" "func" (param f64) (result f64))

        (table $t 10 funcref)
        (type $f_f (func (param f32) (result f32)))
        (type $d_d (func (param f64) (result f64)))
        (elem (i32.const 0) $f32 $f64)

        (func (export "f32") (param f32) (result f32)
            local.get 0
            call $f32
        )
        (func (export "f32_t") (param f32) (result f32)
            local.get 0
            i32.const 0
            call_indirect $f_f
        )
        (func (export "f64") (param f64) (result f64)
            local.get 0
            call $f64
        )
        (func (export "f64_t") (param f64) (result f64)
            local.get 0
            i32.const 1
            call_indirect $d_d
        )
    )`, imports).exports;

    for (let v of values) {
        assertEq(i.f32(v), f32(v));
        assertEq(i.f32_t(v), f32(v));
        assertEq(i.f64(v), f64(v));
        assertEq(i.f64_t(v), f64(v));
    }
}

function binary(name) {
    print(name);

    let imports = {
        math: {
            func: Math[name]
        }
    };

    let f32 = (x, y) => Math.fround(Math[name](Math.fround(x), Math.fround(y)));
    let f64 = Math[name];

    let i = wasmEvalText(`(module
        (import $f32 "math" "func" (param f32) (param f32) (result f32))
        (import $f64 "math" "func" (param f64) (param f64) (result f64))

        (table $t 10 funcref)
        (type $ff_f (func (param f32) (param f32) (result f32)))
        (type $dd_d (func (param f64) (param f64) (result f64)))
        (elem (i32.const 0) $f32 $f64)

        (func (export "f32") (param f32) (param f32) (result f32)
            local.get 0
            local.get 1
            call $f32
        )
        (func (export "f32_t") (param f32) (param f32) (result f32)
            local.get 0
            local.get 1
            i32.const 0
            call_indirect $ff_f
        )
        (func (export "f64") (param f64) (param f64) (result f64)
            local.get 0
            local.get 1
            call $f64
        )
        (func (export "f64_t") (param f64) (param f64) (result f64)
            local.get 0
            local.get 1
            i32.const 1
            call_indirect $dd_d
        )
    )`, imports).exports;

    for (let v of values) {
        for (let w of values) {
            assertEq(i.f32(v, w), f32(v, w));
            assertEq(i.f64(v, w), f64(v, w));
        }
    }
}

unary('sin');
unary('sin');
unary('tan');
unary('cos');
unary('exp');
unary('log');
unary('asin');
unary('atan');
unary('acos');
unary('log10');
unary('log2');
unary('log1p');
unary('expm1');
unary('sinh');
unary('tanh');
unary('cosh');
unary('asinh');
unary('atanh');
unary('acosh');
unary('sign');
unary('trunc');
unary('cbrt');

binary('atan2');
binary('hypot');
binary('pow');

print('done');
