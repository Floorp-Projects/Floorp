// |jit-test| skip-if: !wasmGcEnabled()

// Moving a JS value through a wasm externref is a pair of boxing/unboxing
// conversions that leaves the value unchanged.  There are many cases,
// along these axes:
//
//  - global variables, see externref-boxing.js
//  - tables, see externref-boxing.js
//  - function parameters and returns, see externref-boxing.js
//  - struct fields [for the gc feature], this file
//  - WasmGcObject fields when we construct with the externref type; this file

// Struct fields of externref type can receive their value in these ways:
//
// - the struct.new and struct.set instructions
// - storing into mutable fields from JS
// - from a constructor called from JS
//
// Struct fields can be read in these ways:
//
// - the struct.get instruction
// - reading the field from JS
//
// We're especially interested in two cases: where JS stores a non-object value
// into a field, in this case there should be boxing; and where JS reads a
// non-pointer value from a field, in this case there should be unboxing.

// Write with struct.new, read with the JS getter

for (let v of WasmExternrefValues)
{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut externref))))
           (func (export "make") (param $v externref) (result eqref)
             (struct.new $S (local.get $v))))`);
    let x = ins.exports.make(v);
    assertEq(wasmGcReadField(x, 0), v);
}

// Try to make sure externrefs are properly traced

{
    let fields = iota(10).map(() => `(field externref)`).join(' ');
    let params = iota(10).map((i) => `(param $${i} externref)`).join(' ');
    let args = iota(10).map((i) => `(local.get $${i})`).join(' ');
    let txt = `(module
                 (type $S (struct ${fields}))
                 (func (export "make") ${params} (result eqref)
                   (struct.new $S ${args})))`;
    let ins = wasmEvalText(txt);
    let x = ins.exports.make({x:0}, {x:1}, {x:2}, {x:3}, {x:4}, {x:5}, {x:6}, {x:7}, {x:8}, {x:9})
    gc('shrinking');
    assertEq(typeof wasmGcReadField(x, 0), "object");
    assertEq(wasmGcReadField(x, 0).x, 0);
    assertEq(typeof wasmGcReadField(x, 1), "object");
    assertEq(wasmGcReadField(x, 1).x, 1);
    assertEq(typeof wasmGcReadField(x, 2), "object");
    assertEq(wasmGcReadField(x, 2).x, 2);
    assertEq(typeof wasmGcReadField(x, 3), "object");
    assertEq(wasmGcReadField(x, 3).x, 3);
    assertEq(typeof wasmGcReadField(x, 4), "object");
    assertEq(wasmGcReadField(x, 4).x, 4);
    assertEq(typeof wasmGcReadField(x, 5), "object");
    assertEq(wasmGcReadField(x, 5).x, 5);
    assertEq(typeof wasmGcReadField(x, 6), "object");
    assertEq(wasmGcReadField(x, 6).x, 6);
    assertEq(typeof wasmGcReadField(x, 7), "object");
    assertEq(wasmGcReadField(x, 7).x, 7);
    assertEq(typeof wasmGcReadField(x, 8), "object");
    assertEq(wasmGcReadField(x, 8).x, 8);
    assertEq(typeof wasmGcReadField(x, 9), "object");
    assertEq(wasmGcReadField(x, 9).x, 9);
}
