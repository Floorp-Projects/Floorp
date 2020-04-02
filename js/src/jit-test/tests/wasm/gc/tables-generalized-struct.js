// |jit-test| skip-if: !wasmReftypesEnabled() || !wasmGcEnabled() || wasmCompileMode() != 'baseline'

// table.set in bounds with i32 x anyref - works, no value generated
// table.set with (ref opt T) - works
// table.set with null - works
// table.set out of bounds - fails

{
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 anyref)
           (type $dummy (struct (field i32)))
           (func (export "set_anyref") (param i32) (param anyref)
             (table.set (local.get 0) (local.get 1)))
           (func (export "set_null") (param i32)
             (table.set (local.get 0) (ref.null)))
           (func (export "set_ref") (param i32) (param anyref)
             (table.set (local.get 0) (struct.narrow anyref (ref opt $dummy) (local.get 1))))
           (func (export "make_struct") (result anyref)
             (struct.new $dummy (i32.const 37))))`);
    let x = {};
    ins.exports.set_anyref(3, x);
    assertEq(ins.exports.t.get(3), x);
    ins.exports.set_null(3);
    assertEq(ins.exports.t.get(3), null);
    let dummy = ins.exports.make_struct();
    ins.exports.set_ref(5, dummy);
    assertEq(ins.exports.t.get(5), dummy);

    assertErrorMessage(() => ins.exports.set_anyref(10, x), RangeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.set_anyref(-1, x), RangeError, /index out of bounds/);
}

// table.grow on table of anyref with non-null ref value

{
    let ins = wasmEvalText(
        `(module
          (type $S (struct (field i32) (field f64)))
          (table (export "t") 2 anyref)
          (func (export "f") (result i32)
           (table.grow (struct.new $S (i32.const 0) (f64.const 3.14)) (i32.const 1))))`);
    assertEq(ins.exports.t.length, 2);
    assertEq(ins.exports.f(), 2);
    assertEq(ins.exports.t.length, 3);
    assertEq(typeof ins.exports.t.get(2), "object");
}

