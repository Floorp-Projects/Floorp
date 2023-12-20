// |jit-test| --wasm-gc; skip-if: !wasmCachingEnabled() || !wasmGcEnabled()

const code = wasmTextToBinary(`(module
  (type $t (struct (field i32) (field anyref)))
  (type $t2 (struct (field f32) (field externref)))
  (func (export "test") (param externref) (result externref)
    i32.const 42
    f32.const 1.0
    local.get 0
    struct.new $t2
    struct.new $t
    extern.convert_any
  )

  (func (export "check") (param externref) (result externref)
    local.get 0
    any.convert_extern
    ref.cast (ref $t)
    struct.get $t 1
    ref.cast (ref $t2)
    struct.get $t2 1
  )
)`);

function instantiateCached(code, imports) {
  // Cache the code.
  wasmCompileInSeparateProcess(code);
  // Load from cache.
  let m = wasmCompileInSeparateProcess(code);
  assertEq(wasmLoadedFromCache(m), true);
  return new WebAssembly.Instance(m, imports);
}

const {test,check} = instantiateCached(code).exports;
let obj = test({i:1});
gc();
assertEq(check(obj).i, 1);
