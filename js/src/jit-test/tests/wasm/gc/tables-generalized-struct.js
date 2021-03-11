// |jit-test| skip-if: !wasmGcEnabled()

// table.set in bounds with i32 x eqref - works, no value generated
// table.set with (ref null T) - works
// table.set with null - works
// table.set out of bounds - fails

{
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 eqref)
           (type $dummy (struct (field i32)))
           (func (export "set_eqref") (param i32) (param eqref)
             (table.set (local.get 0) (local.get 1)))
           (func (export "set_null") (param i32)
             (table.set (local.get 0) (ref.null eq)))
           (func (export "set_ref") (param i32) (param eqref)
             (table.set (local.get 0) (ref.cast eq $dummy (local.get 1) (rtt.canon $dummy))))
           (func (export "make_struct") (result eqref)
             (struct.new_with_rtt $dummy (i32.const 37) (rtt.canon $dummy))))`);
    let a = ins.exports.make_struct();
    ins.exports.set_eqref(3, a);
    assertEq(ins.exports.t.get(3), a);
    ins.exports.set_null(3);
    assertEq(ins.exports.t.get(3), null);
    let b = ins.exports.make_struct();
    ins.exports.set_ref(5, b);
    assertEq(ins.exports.t.get(5), b);

    assertErrorMessage(() => ins.exports.set_eqref(10, a), WebAssembly.RuntimeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.set_eqref(-1, a), WebAssembly.RuntimeError, /index out of bounds/);
}

// table.grow on table of eqref with non-null ref value

{
    let ins = wasmEvalText(
        `(module
          (type $S (struct (field i32) (field f64)))
          (table (export "t") 2 eqref)
          (func (export "f") (result i32)
           (table.grow (struct.new_with_rtt $S (i32.const 0) (f64.const 3.14) (rtt.canon $S)) (i32.const 1))))`);
    assertEq(ins.exports.t.length, 2);
    assertEq(ins.exports.f(), 2);
    assertEq(ins.exports.t.length, 3);
    assertEq(typeof ins.exports.t.get(2), "object");
}

