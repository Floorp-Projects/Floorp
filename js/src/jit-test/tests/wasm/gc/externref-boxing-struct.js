// |jit-test| skip-if: !wasmGcEnabled()

// Moving a JS value through a wasm externref is a pair of boxing/unboxing
// conversions that leaves the value unchanged.  There are many cases,
// along these axes:
//
//  - global variables, see externref-boxing.js
//  - tables, see externref-boxing.js
//  - function parameters and returns, see externref-boxing.js
//  - struct fields [for the gc feature], this file
//  - TypedObject fields when we construct with the externref type; this file

// Struct fields of externref type can receive their value in these ways:
//
// - the struct.new_with_rtt and struct.set instructions
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

// Write with struct.new_with_rtt, read with the JS getter

for (let v of WasmExternrefValues)
{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut externref))))
           (func (export "make") (param $v externref) (result eqref)
             (struct.new_with_rtt $S (local.get $v) (rtt.canon $S))))`);
    let x = ins.exports.make(v);
    assertEq(x[0], v);
}

// Try to make sure externrefs are properly traced

{
    let fields = iota(10).map(() => `(field externref)`).join(' ');
    let params = iota(10).map((i) => `(param $${i} externref)`).join(' ');
    let args = iota(10).map((i) => `(local.get $${i})`).join(' ');
    let txt = `(module
                 (type $S (struct ${fields}))
                 (func (export "make") ${params} (result eqref)
                   (struct.new_with_rtt $S ${args} (rtt.canon $S))))`;
    let ins = wasmEvalText(txt);
    let x = ins.exports.make({x:0}, {x:1}, {x:2}, {x:3}, {x:4}, {x:5}, {x:6}, {x:7}, {x:8}, {x:9})
    gc('shrinking');
    assertEq(typeof x[0], "object");
    assertEq(x[0].x, 0);
    assertEq(typeof x[1], "object");
    assertEq(x[1].x, 1);
    assertEq(typeof x[2], "object");
    assertEq(x[2].x, 2);
    assertEq(typeof x[3], "object");
    assertEq(x[3].x, 3);
    assertEq(typeof x[4], "object");
    assertEq(x[4].x, 4);
    assertEq(typeof x[5], "object");
    assertEq(x[5].x, 5);
    assertEq(typeof x[6], "object");
    assertEq(x[6].x, 6);
    assertEq(typeof x[7], "object");
    assertEq(x[7].x, 7);
    assertEq(typeof x[8], "object");
    assertEq(x[8].x, 8);
    assertEq(typeof x[9], "object");
    assertEq(x[9].x, 9);
}

function iota(k) {
    let a = new Array(k);
    for ( let i=0 ; i < k; i++ )
        a[i] = i;
    return a;
}
