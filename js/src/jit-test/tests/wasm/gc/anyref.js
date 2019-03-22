// |jit-test| skip-if: !wasmReftypesEnabled()

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
        ref.null
        i32.const 42
        select
    )
)`), CompileError, /select operand types/);

assertErrorMessage(() => wasmEvalText(`(module
    (func (result i32)
        ref.null
        if
            i32.const 42
        end
    )
)`), CompileError, mismatchError('nullref', 'i32'));


// Basic compilation tests.

let simpleTests = [
    "(module (func (drop (ref.null))))",
    "(module (func $test (local anyref)))",
    "(module (func $test (param anyref)))",
    "(module (func $test (result anyref) (ref.null)))",
    "(module (func $test (block anyref (unreachable)) unreachable))",
    "(module (func $test (local anyref) (result i32) (ref.is_null (local.get 0))))",
    `(module (import "a" "b" (param anyref)))`,
    `(module (import "a" "b" (result anyref)))`,
    `(module (global anyref (ref.null)))`,
    `(module (global (mut anyref) (ref.null)))`,
];

for (let src of simpleTests) {
    wasmEvalText(src, {a:{b(){}}});
    assertEq(validate(wasmTextToBinary(src)), true);
}

// Basic behavioral tests.

let { exports } = wasmEvalText(`(module
    (func (export "is_null") (result i32)
        ref.null
        ref.is_null
    )

    (func $sum (result i32) (param i32)
        local.get 0
        i32.const 42
        i32.add
    )

    (func (export "is_null_spill") (result i32)
        ref.null
        i32.const 58
        call $sum
        drop
        ref.is_null
    )

    (func (export "is_null_local") (result i32) (local anyref)
        ref.null
        local.set 0
        i32.const 58
        call $sum
        drop
        local.get 0
        ref.is_null
    )
    )`);

assertEq(exports.is_null(), 1);
assertEq(exports.is_null_spill(), 1);
assertEq(exports.is_null_local(), 1);

// Anyref param and result in wasm functions.

exports = wasmEvalText(`(module
    (func (export "is_null") (result i32) (param $ref anyref)
        local.get $ref
        ref.is_null
    )

    (func (export "ref_or_null") (result anyref) (param $ref anyref) (param $selector i32)
        local.get $ref
        ref.null
        local.get $selector
        select
    )

    (func $recursive (export "nested") (result anyref) (param $ref anyref) (param $i i32)
        ;; i == 10 => ret $ref
        local.get $i
        i32.const 10
        i32.eq
        if
            local.get $ref
            return
        end

        local.get $ref

        local.get $i
        i32.const 1
        i32.add

        call $recursive
    )
)`).exports;

assertEq(exports.is_null(undefined), 0);
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

// Make sure grow-memory isn't blocked by the lack of gc.
(function() {
    assertEq(wasmEvalText(`(module
    (memory 0 64)
    (func (export "f") (param anyref) (result i32)
        i32.const 10
        memory.grow
        drop
        memory.size
    )
)`).exports.f({}), 10);
})();

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

assertJoin("(block anyref local.get $ref)");
assertJoin("(block $out anyref local.get $ref br $out)");
assertJoin("(loop anyref local.get $ref)");

assertJoin(`(block $out anyref (loop $top anyref
    local.get $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.eq
    if
        local.get $ref
        return
    end
    br $top))
`);

assertJoin(`(block $out (loop $top
    local.get $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.le_s
    if
        br $top
    else
        local.get $ref
        return
    end
    )) unreachable
`);

assertJoin(`(block $out anyref (loop $top
    local.get $ref
    local.get $i
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
    local.get $ref
    local.get $i
    i32.const 1
    i32.add
    tee_local $i
    br_table $unreachable $out
    ) unreachable))
`);

let x = { i: 42 }, y = { f: 53 };
exports = wasmEvalText(`(module
    (func (export "test") (param $lhs anyref) (param $rhs anyref) (param $i i32) (result anyref)
        local.get $lhs
        local.get $rhs
        local.get $i
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
        local.get $y
        local.get $x
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
        global.get $count_f
        i32.add
        global.set $count_f

        local.get $param
        call $augment
    )

    (func $g (param $param anyref) (result anyref)
        i32.const 1
        global.get $count_g
        i32.add
        global.set $count_g

        local.get $param
        call $mirror
    )

    (table (export "table") 10 funcref)
    (elem (i32.const 0) $f $g $mirror $augment)
    (type $table_type (func (param anyref) (result anyref)))

    (func (export "call_indirect") (param $i i32) (param $ref anyref) (result anyref)
        local.get $ref
        local.get $i
        call_indirect $table_type
    )

    (func (export "count_f") (result i32) global.get $count_f)
    (func (export "count_g") (result i32) global.get $count_g)
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

// Globals.

// Anyref globals in wasm modules.

assertErrorMessage(() => wasmEvalText(`(module (global (import "glob" "anyref") anyref))`, { glob: { anyref: new WebAssembly.Global({ value: 'i32' }, 42) } }),
    WebAssembly.LinkError,
    /imported global type mismatch/);

assertErrorMessage(() => wasmEvalText(`(module (global (import "glob" "i32") i32))`, { glob: { i32: {} } }),
    WebAssembly.LinkError,
    /import object field 'i32' is not a Number/);

imports = {
    constants: {
        imm_null: null,
        imm_bread: new Baguette(321),
        mut_null: new WebAssembly.Global({ value: "anyref", mutable: true }, null),
        mut_bread: new WebAssembly.Global({ value: "anyref", mutable: true }, new Baguette(123))
    }
};

exports = wasmEvalText(`(module
    (global $g_imp_imm_null  (import "constants" "imm_null") anyref)
    (global $g_imp_imm_bread (import "constants" "imm_bread") anyref)

    (global $g_imp_mut_null   (import "constants" "mut_null") (mut anyref))
    (global $g_imp_mut_bread  (import "constants" "mut_bread") (mut anyref))

    (global $g_imm_null     anyref (ref.null))
    (global $g_imm_getglob  anyref (global.get $g_imp_imm_bread))
    (global $g_mut         (mut anyref) (ref.null))

    (func (export "imm_null")      (result anyref) global.get $g_imm_null)
    (func (export "imm_getglob")   (result anyref) global.get $g_imm_getglob)

    (func (export "imp_imm_null")  (result anyref) global.get $g_imp_imm_null)
    (func (export "imp_imm_bread") (result anyref) global.get $g_imp_imm_bread)
    (func (export "imp_mut_null")  (result anyref) global.get $g_imp_mut_null)
    (func (export "imp_mut_bread") (result anyref) global.get $g_imp_mut_bread)

    (func (export "set_imp_null")  (param anyref) local.get 0 global.set $g_imp_mut_null)
    (func (export "set_imp_bread") (param anyref) local.get 0 global.set $g_imp_mut_bread)

    (func (export "set_mut") (param anyref) local.get 0 global.set $g_mut)
    (func (export "get_mut") (result anyref) global.get $g_mut)
)`, imports).exports;

assertEq(exports.imp_imm_null(), imports.constants.imm_null);
assertEq(exports.imp_imm_bread(), imports.constants.imm_bread);

assertEq(exports.imm_null(), null);
assertEq(exports.imm_getglob(), imports.constants.imm_bread);

assertEq(exports.imp_mut_null(), imports.constants.mut_null.value);
assertEq(exports.imp_mut_bread(), imports.constants.mut_bread.value);

let brandNewBaguette = new Baguette(1000);
exports.set_imp_null(brandNewBaguette);
assertEq(exports.imp_mut_null(), brandNewBaguette);
assertEq(exports.imp_mut_bread(), imports.constants.mut_bread.value);

exports.set_imp_bread(null);
assertEq(exports.imp_mut_null(), brandNewBaguette);
assertEq(exports.imp_mut_bread(), null);

assertEq(exports.get_mut(), null);
let glutenFreeBaguette = new Baguette("calories-free bread");
exports.set_mut(glutenFreeBaguette);
assertEq(exports.get_mut(), glutenFreeBaguette);
assertEq(exports.get_mut().calories, "calories-free bread");

// Make sure that dead code doesn't prevent compilation.
wasmEvalText(
    `(module
       (func
         (return)
         (ref.null)
         (drop)
        )
    )`);

wasmEvalText(
    `(module
       (func (param anyref)
         (return)
         (ref.is_null (get_local 0))
         (drop)
        )
    )`);
