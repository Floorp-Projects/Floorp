// Ensure that if gc types aren't enabled, test cases properly fail.

if (!wasmGcEnabled()) {
    quit(0);
}

// Dummy constructor.
function Baguette(calories) {
    this.calories = calories;
}

// Type checking.

const { validate, CompileError } = WebAssembly;

assertErrorMessage(() => wasmEvalText(`(module
    (func (result anyref)
        i32.const 42
    )
)`), CompileError, mismatchError('i32', 'anyref'));

assertErrorMessage(() => wasmEvalText(`(module
    (func (result anyref)
        i32.const 0
        ref.null anyref
        i32.const 42
        select
    )
)`), CompileError, /select operand types/);

assertErrorMessage(() => wasmEvalText(`(module
    (func (result i32)
        ref.null anyref
        if
            i32.const 42
        end
    )
)`), CompileError, mismatchError('anyref', 'i32'));


// Basic compilation tests.

let simpleTests = [
    "(module (func (drop (ref.null anyref))))",
    "(module (func $test (local anyref)))",
    "(module (func $test (param anyref)))",
    "(module (func $test (result anyref) (ref.null anyref)))",
    "(module (func $test (block anyref (unreachable)) unreachable))",
    "(module (func $test (local anyref) (result i32) (ref.is_null (get_local 0))))",
    `(module (import "a" "b" (param anyref)))`,
    `(module (import "a" "b" (result anyref)))`,
];

for (let src of simpleTests) {
    wasmEvalText(src, {a:{b(){}}});
    assertEq(validate(wasmTextToBinary(src)), true);
}

// Basic behavioral tests.

let { exports } = wasmEvalText(`(module
    (func (export "is_null") (result i32)
        ref.null anyref
        ref.is_null
    )

    (func $sum (result i32) (param i32)
        get_local 0
        i32.const 42
        i32.add
    )

    (func (export "is_null_spill") (result i32)
        ref.null anyref
        i32.const 58
        call $sum
        drop
        ref.is_null
    )

    (func (export "is_null_local") (result i32) (local anyref)
        ref.null anyref
        set_local 0
        i32.const 58
        call $sum
        drop
        get_local 0
        ref.is_null
    )
)`);

assertEq(exports.is_null(), 1);
assertEq(exports.is_null_spill(), 1);
assertEq(exports.is_null_local(), 1);

// Anyref param and result in wasm functions.

exports = wasmEvalText(`(module
    (func (export "is_null") (result i32) (param $ref anyref)
        get_local $ref
        ref.is_null
    )

    (func (export "ref_or_null") (result anyref) (param $ref anyref) (param $selector i32)
        get_local $ref
        ref.null anyref
        get_local $selector
        select
    )

    (func $recursive (export "nested") (result anyref) (param $ref anyref) (param $i i32)
        ;; i == 10 => ret $ref
        get_local $i
        i32.const 10
        i32.eq
        if
            get_local $ref
            return
        end

        get_local $ref

        get_local $i
        i32.const 1
        i32.add

        call $recursive
    )
)`).exports;

assertErrorMessage(() => exports.is_null(undefined), TypeError, "can't convert undefined to object");
assertEq(exports.is_null(null), 1);
assertEq(exports.is_null({}), 0);
assertEq(exports.is_null("hi"), 0);
assertEq(exports.is_null(3), 0);
assertEq(exports.is_null(3.5), 0);
assertEq(exports.is_null(true), 0);
assertEq(exports.is_null(Symbol("croissant")), 0);
assertEq(exports.is_null(new Baguette(100)), 0);

let baguette = new Baguette(42);
assertEq(exports.ref_or_null(null, 0), null);
assertEq(exports.ref_or_null(baguette, 0), null);

let ref = exports.ref_or_null(baguette, 1);
assertEq(ref, baguette);
assertEq(ref.calories, baguette.calories);

ref = exports.nested(baguette, 0);
assertEq(ref, baguette);
assertEq(ref.calories, baguette.calories);

// More interesting use cases about control flow joins.

function assertJoin(body) {
    let val = { i: -1 };
    assertEq(wasmEvalText(`(module
        (func (export "test") (param $ref anyref) (param $i i32) (result anyref)
            ${body}
        )
    )`).exports.test(val), val);
    assertEq(val.i, -1);
}

assertJoin("(block anyref get_local $ref)");
assertJoin("(block $out anyref get_local $ref br $out)");
assertJoin("(loop anyref get_local $ref)");

assertJoin(`(block $out anyref (loop $top anyref
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.eq
    if
        get_local $ref
        return
    end
    br $top))
`);

assertJoin(`(block $out (loop $top
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.le_s
    if
        br $top
    else
        get_local $ref
        return
    end
    )) unreachable
`);

assertJoin(`(block $out anyref (loop $top
    get_local $ref
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.eq
    br_if $out
    br $top
    ) unreachable)
`);

assertJoin(`(block $out anyref (block $unreachable anyref (loop $top
    get_local $ref
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    br_table $unreachable $out
    ) unreachable))
`);

let x = { i: 42 }, y = { f: 53 };
exports = wasmEvalText(`(module
    (func (export "test") (param $lhs anyref) (param $rhs anyref) (param $i i32) (result anyref)
        get_local $lhs
        get_local $rhs
        get_local $i
        select
    )
)`).exports;

let result = exports.test(x, y, 0);
assertEq(result, y);
assertEq(result.i, undefined);
assertEq(result.f, 53);
assertEq(x.i, 42);

result = exports.test(x, y, 1);
assertEq(result, x);
assertEq(result.i, 42);
assertEq(result.f, undefined);
assertEq(y.f, 53);

// Anyref in params/result of imported functions.

let firstBaguette = new Baguette(13),
    secondBaguette = new Baguette(37);

let imports = {
    i: 0,
    myBaguette: null,
    funcs: {
        param(x) {
            if (this.i === 0) {
                assertEq(x, firstBaguette);
                assertEq(x.calories, 13);
                assertEq(secondBaguette !== null, true);
            } else if (this.i === 1 || this.i === 2) {
                assertEq(x, secondBaguette);
                assertEq(x.calories, 37);
                assertEq(firstBaguette !== null, true);
            } else if (this.i === 3) {
                assertEq(x, null);
            } else {
                firstBaguette = null;
                secondBaguette = null;
                gc(); // evil mode
            }
            this.i++;
        },
        ret() {
            return imports.myBaguette;
        }
    }
};

exports = wasmEvalText(`(module
    (import $ret "funcs" "ret" (result anyref))
    (import $param "funcs" "param" (param anyref))

    (func (export "param") (param $x anyref) (param $y anyref)
        get_local $y
        get_local $x
        call $param
        call $param
    )

    (func (export "ret") (result anyref)
        call $ret
    )
)`, imports).exports;

exports.param(firstBaguette, secondBaguette);
exports.param(secondBaguette, null);
exports.param(firstBaguette, secondBaguette);

imports.myBaguette = null;
assertEq(exports.ret(), null);

imports.myBaguette = new Baguette(1337);
assertEq(exports.ret(), imports.myBaguette);

// Check lazy stubs generation.

exports = wasmEvalText(`(module
    (import $mirror "funcs" "mirror" (param anyref) (result anyref))
    (import $augment "funcs" "augment" (param anyref) (result anyref))

    (global $count_f (mut i32) (i32.const 0))
    (global $count_g (mut i32) (i32.const 0))

    (func $f (param $param anyref) (result anyref)
        i32.const 1
        get_global $count_f
        i32.add
        set_global $count_f

        get_local $param
        call $augment
    )

    (func $g (param $param anyref) (result anyref)
        i32.const 1
        get_global $count_g
        i32.add
        set_global $count_g

        get_local $param
        call $mirror
    )

    (table (export "table") 10 anyfunc)
    (elem (i32.const 0) $f $g $mirror $augment)
    (type $table_type (func (param anyref) (result anyref)))

    (func (export "call_indirect") (param $i i32) (param $ref anyref) (result anyref)
        get_local $ref
        get_local $i
        call_indirect $table_type
    )

    (func (export "count_f") (result i32) get_global $count_f)
    (func (export "count_g") (result i32) get_global $count_g)
)`, {
    funcs: {
        mirror(x) {
            return x;
        },
        augment(x) {
            x.i++;
            x.newProp = "hello";
            return x;
        }
    }
}).exports;

x = { i: 19 };
assertEq(exports.table.get(0)(x), x);
assertEq(x.i, 20);
assertEq(x.newProp, "hello");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 0);

x = { i: 21 };
assertEq(exports.table.get(1)(x), x);
assertEq(x.i, 21);
assertEq(typeof x.newProp, "undefined");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 1);

x = { i: 22 };
assertEq(exports.table.get(2)(x), x);
assertEq(x.i, 22);
assertEq(typeof x.newProp, "undefined");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 1);

x = { i: 23 };
assertEq(exports.table.get(3)(x), x);
assertEq(x.i, 24);
assertEq(x.newProp, "hello");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 1);
