load(libdir + "wasm.js");

// ----------------------------------------------------------------------------
// if_else

// Condition is an int32
assertErrorMessage(() => wasmEvalText('(module (func (local f32) (if (get_local 0) (i32.const 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (local f32) (if_else (get_local 0) (i32.const 1) (i32.const 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (local f64) (if_else (get_local 0) (i32.const 1) (i32.const 0))))'), TypeError, mismatchError("f64", "i32"));
wasmEvalText('(module (func (local i32) (if (get_local 0) (nop))) (export "" 0))');
wasmEvalText('(module (func (local i32) (if_else (get_local 0) (nop) (nop))) (export "" 0))');

// Expression values types are consistent
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (local f32) (if_else (i32.const 42) (get_local 0) (i32.const 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (local f64) (if_else (i32.const 42) (i32.const 0) (get_local 0))))'), TypeError, mismatchError("f64", "i32"));
assertEq(wasmEvalText('(module (func (result i32) (if_else (i32.const 42) (i32.const 1) (i32.const 2))) (export "" 0))')(), 1);
assertEq(wasmEvalText('(module (func (result i32) (if_else (i32.const 0) (i32.const 1) (i32.const 2))) (export "" 0))')(), 2);

// If we don't yield, sub expressions types don't have to match
assertEq(wasmEvalText('(module (func (if_else (i32.const 42) (i32.const 1) (i32.const 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (param f32) (if_else (i32.const 42) (i32.const 1) (get_local 0))) (export "" 0))')(13.37), undefined);

// Sub-expression values are returned
assertEq(wasmEvalText(`(module
    (func
        (result i32)
        (if_else
            (i32.const 42)
            (block
                (
                    if_else
                    (block
                        (i32.const 3)
                        (i32.const 5)
                        (i32.const 0)
                    )
                    (i32.const 1)
                    (i32.const 2)
                )
            )
            (i32.const 0)
        )
    )
    (export "" 0)
)`)(), 2);

// The if (resp. else) branch is taken iff the condition is true (resp. false)
counter = 0;
var imports = { inc() { counter++ } };
assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (result i32)
        (if_else
            (i32.const 42)
            (i32.const 1)
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), 1);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (result i32)
        (if_else
            (i32.const 0)
            (call_import 0)
            (i32.const 1)
        )
    )
    (export "" 0)
)`, imports)(), 1);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (if
            (i32.const 0)
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), undefined);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (if
            (i32.const 1)
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), undefined);
assertEq(counter, 1);

// One can chain if_else with if/if_else
counter = 0;
assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (result i32)
        (if_else
            (i32.const 1)
            (if_else
                (i32.const 2)
                (if_else
                    (i32.const 3)
                    (if_else
                        (i32.const 0)
                        (call_import 0)
                        (i32.const 42)
                    )
                    (call_import 0)
                )
                (call_import 0)
            )
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), 42);
assertEq(counter, 0);

// "if" doesn't return an expression value
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (if (i32.const 42) (i32.const 0))))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (if_else (i32.const 1) (i32.const 0) (if (i32.const 1) (i32.const 1)))))'), TypeError, mismatchError("void", "i32"));
wasmEvalText('(module (func (if_else (i32.const 1) (i32.const 0) (if (i32.const 1) (i32.const 1)))))');
