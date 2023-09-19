// |jit-test| skip-if: !wasmCachingEnabled()

load(libdir + "wasm-caching.js");
load(libdir + "wasm-binary.js");

testCached(`(module
    (func $test (param i64) (result f64)
        local.get 0
        f64.convert_i64_u
    )
    (func (export "run") (result i32)
        i64.const 1
        call $test
        f64.const 1
        f64.eq
    )
)`,
    undefined,
    i => { assertEq(i.exports.run(), 1); }
);

testCached(
    `(module
       (type $long-serialized-type (func (param
          i64 i32 f64 f32 i64 i32 f64 f32 i64 i32 f64 f32 i64 i32 f64 f32))) 
       (func (export "run") (result i32)
         (i32.const 42)))`,
    undefined,
    i => { assertEq(i.exports.run(), 42); }
);

testCached(
    `(module
       (type $T (func (result i32)))
       (func $t1 (import "" "t1") (type $T))
       (func $t2 (import "" "t2") (type $T))
       (func $t3 (type $T) (i32.const 30))
       (func $t4 (type $T) (i32.const 40))
       (table funcref (elem $t1 $t2 $t3 $t4))
       (func (export "run") (param i32) (result i32)
         (call_indirect (type $T) (local.get 0))))`,
    {'':{ t1() { return 10 }, t2() { return 20 } }},
    i => {
        assertEq(i.exports.run(0), 10);
        assertEq(i.exports.run(1), 20);
        assertEq(i.exports.run(2), 30);
        assertEq(i.exports.run(3), 40);
    }
);

testCached(
  moduleWithSections([
    sigSection([{args:[], ret:VoidCode}]),
    declSection([0]),
    exportSection([{funcIndex:0, name:"run"}]),
    bodySection([funcBody({locals:[], body:[UnreachableCode]})]),
    nameSection([funcNameSubsection([{name:"wee"}])])
  ]),
  undefined,
  i => assertErrorMessage(() => i.exports.run(), RuntimeError, /unreachable/)
);

// Note: a fuller behavioral test of caching is in bench/wasm_box2d.js.
