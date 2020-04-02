// |jit-test| skip-if: !wasmGcEnabled() || wasmCompileMode() != 'baseline'

// Moving a JS value through a wasm anyref is a pair of boxing/unboxing
// conversions that leaves the value unchanged.  There are many cases,
// along these axes:
//
//  - global variables, see anyref-boxing.js
//  - tables, see anyref-boxing.js
//  - function parameters and returns, see anyref-boxing.js
//  - struct fields [for the gc feature], this file
//  - TypedObject fields when we construct with the anyref type; this file

let VALUES = [null,
              undefined,
              true,
              false,
              {x:1337},
              ["abracadabra"],
              1337,
              13.37,
              "hi",
              37n,
              Symbol("status"),
              () => 1337];

// Struct fields of anyref type can receive their value in these ways:
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

for (let v of VALUES)
{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (param $v anyref) (result anyref)
             (struct.new $S (local.get $v))))`);
    let x = ins.exports.make(v);
    assertEq(x._0, v);
}

// Write with JS setter, read with struct.get

for (let v of VALUES)
{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (result anyref)
             (struct.new $S (ref.null)))
           (func (export "get") (param $o anyref) (result anyref)
             (struct.get $S 0 (struct.narrow anyref (ref opt $S) (local.get $o)))))`);
    let x = ins.exports.make();
    x._0 = v;
    assertEq(ins.exports.get(x), v);
}

// Write with JS constructor, read with struct.get

for (let v of VALUES)
{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (result anyref)
             (struct.new $S (ref.null)))
           (func (export "get") (param $o anyref) (result anyref)
             (struct.get $S 0 (struct.narrow anyref (ref opt $S) (local.get $o)))))`);
    let constructor = ins.exports.make().constructor;
    let x = new constructor({_0: v});
    assertEq(ins.exports.get(x), v);
}

// TypedObject.WasmAnyRef exists and is an identity operation

assertEq(typeof TypedObject.WasmAnyRef, "function");
for (let v of VALUES) {
    assertEq(TypedObject.WasmAnyRef(v), v);
}

// We can create structures with WasmAnyRef type, if we want.
// It's not totally obvious that we want to allow this long-term though.

{
    let constructor = new TypedObject.StructType({_0: TypedObject.WasmAnyRef});
    let x = new constructor({_0: 3.25});
    assertEq(x._0, 3.25);
    let y = new constructor({_0: assertEq});
    assertEq(y._0, assertEq);
}

// The default value of an anyref field should be null.

{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (result anyref)
             (struct.new $S (ref.null))))`);
    let constructor = ins.exports.make().constructor;
    let x = new constructor();
    assertEq(x._0, null);
}

// Here we should actually see an undefined value since undefined is
// representable as AnyRef.

{
    let ins = wasmEvalText(
        `(module
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (result anyref)
             (struct.new $S (ref.null))))`);
    let constructor = ins.exports.make().constructor;
    let x = new constructor({});
    assertEq(x._0, undefined);
}

// Contrast the previous case with this, where the conversion of the undefined
// value for the missing field _0 must fail.

{
    let constructor = new TypedObject.StructType({_0: TypedObject.Object});
    assertErrorMessage(() => new constructor({}),
                       TypeError,
                       /can't convert undefined/);
}

// Try to make sure anyrefs are properly traced

{
    let fields = iota(10).map(() => `(field anyref)`).join(' ');
    let params = iota(10).map((i) => `(param $${i} anyref)`).join(' ');
    let args = iota(10).map((i) => `(local.get $${i})`).join(' ');
    let txt = `(module
                 (type $S (struct ${fields}))
                 (func (export "make") ${params} (result anyref)
                   (struct.new $S ${args})))`;
    let ins = wasmEvalText(txt);
    let x = ins.exports.make({x:0}, {x:1}, {x:2}, {x:3}, {x:4}, {x:5}, {x:6}, {x:7}, {x:8}, {x:9})
    gc('shrinking');
    assertEq(typeof x._0, "object");
    assertEq(x._0.x, 0);
    assertEq(typeof x._1, "object");
    assertEq(x._1.x, 1);
    assertEq(typeof x._2, "object");
    assertEq(x._2.x, 2);
    assertEq(typeof x._3, "object");
    assertEq(x._3.x, 3);
    assertEq(typeof x._4, "object");
    assertEq(x._4.x, 4);
    assertEq(typeof x._5, "object");
    assertEq(x._5.x, 5);
    assertEq(typeof x._6, "object");
    assertEq(x._6.x, 6);
    assertEq(typeof x._7, "object");
    assertEq(x._7.x, 7);
    assertEq(typeof x._8, "object");
    assertEq(x._8.x, 8);
    assertEq(typeof x._9, "object");
    assertEq(x._9.x, 9);
}

function iota(k) {
    let a = new Array(k);
    for ( let i=0 ; i < k; i++ )
        a[i] = i;
    return a;
}
