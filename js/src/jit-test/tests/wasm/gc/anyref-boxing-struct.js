// |jit-test| skip-if: !wasmGcEnabled()

// Moving a JS value through a wasm anyref is a pair of boxing/unboxing
// conversions that leaves the value unchanged.  There are many cases,
// along these axes:
//
//  - global variables, see anyref-boxing.js
//  - tables, see anyref-boxing.js
//  - function parameters and returns, see anyref-boxing.js
//  - struct fields [for the gc feature], this file

let VALUES = [null,
              undefined,
              true,
              false,
              {x:1337},
              ["abracadabra"],
              1337,
              13.37,
              "hi",
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
           (gc_feature_opt_in 2)
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (param $v anyref) (result anyref)
             (struct.new $S (get_local $v))))`);
    let x = ins.exports.make(v);
    // FIXME
    // Does not work except for objects, we observe object values even for primitives
    //assertEq(x._0, v);
}

// Write with JS setter, read with struct.get

for (let v of VALUES)
{
    let ins = wasmEvalText(
        `(module
           (gc_feature_opt_in 2)
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (result anyref)
             (struct.new $S (ref.null)))
           (func (export "get") (param $o anyref) (result anyref)
             (struct.get $S 0 (struct.narrow anyref (ref $S) (get_local $o)))))`);
    let x = ins.exports.make();
    // FIXME
    // Does not work except for things that convert to object.
    //x._0 = v;
    //assertEq(ins.exports.get(x), v);
}

// Write with JS constructor, read with struct.get

for (let v of VALUES)
{
    let ins = wasmEvalText(
        `(module
           (gc_feature_opt_in 2)
           (type $S (struct (field $S.x (mut anyref))))
           (func (export "make") (result anyref)
             (struct.new $S (ref.null)))
           (func (export "get") (param $o anyref) (result anyref)
             (struct.get $S 0 (struct.narrow anyref (ref $S) (get_local $o)))))`);
    let constructor = ins.exports.make().constructor;
    // FIXME
    // Does not work except for things that convert to object
    //let x = new constructor({_0: v});
    //assertEq(ins.exports.get(x), v);
}
