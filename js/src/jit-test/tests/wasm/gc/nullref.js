// |jit-test| skip-if: !wasmReftypesEnabled()

// Note, passing a non-null value from JS to a wasm anyref in any way generates
// an error; it does not run valueOf or toString, nor are nullish values such as
// undefined or 0 coerced to null.

let effect = false;
let effectful = { valueOf() { effect = true; },
                  toString() { effect = true; } }

// Parameters, returns, locals, exported functions
{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (func (export "f") (param $p nullref) (result nullref)
          (local $l nullref)
          (local.set $l (local.get $p))
          (local.get $l)))`)));
    assertEq(ins.exports.f(null), null);
    assertErrorMessage(() => ins.exports.f(0), TypeError, /nullref requires a null value/);
    assertErrorMessage(() => ins.exports.f(undefined), TypeError, /nullref requires a null value/);
    effect = false;
    assertErrorMessage(() => ins.exports.f(effectful), TypeError, /nullref requires a null value/);
    assertEq(effect, false);
}

// Imported functions
{
    let valueToReturn;
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "g" (func $g (result nullref)))
        (func (export "f")
          (call $g)
          (drop)))`)),
                                       {m:{g: () => valueToReturn}});
    valueToReturn = null;
    ins.exports.f();            // Should work
    valueToReturn = 0;
    assertErrorMessage(() => ins.exports.f(), TypeError, /nullref requires a null value/);
    valueToReturn = undefined;
    assertErrorMessage(() => ins.exports.f(), TypeError, /nullref requires a null value/);
    valueToReturn = effectful;
    effect = false;
    assertErrorMessage(() => ins.exports.f(), TypeError, /nullref requires a null value/);
    assertEq(effect, false);
}

// Linking functions
{
    let ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (func (export "f") (param nullref) (result nullref)
          (local.get 0)))`)));

    // This should just work
    let ins2 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "f" (func (param nullref) (result nullref))))`)),
                                        {m:ins1.exports});

    // Type matching at linking
    assertErrorMessage(() => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "f" (func (param anyref) (result nullref))))`)),
                                                      {m:ins1.exports}),
                       WebAssembly.LinkError,
                       /signature mismatch/);

    assertErrorMessage(() => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "f" (func (param nullref) (result anyref))))`)),
                                                      {m:ins1.exports}),
                       WebAssembly.LinkError,
                       /signature mismatch/);
}

// Tables and segments
new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (elem (table $t) (i32.const 4) nullref (ref.null) (ref.null) (ref.null)))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (func $f)
    (elem (table $t) (i32.const 4) nullref (ref.func $f) (ref.null) (ref.null)))`)),
                   WebAssembly.CompileError,
                   /initializer type must be subtype of element type/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (func $f)
    (elem (table $t) (i32.const 4) func $f))`)),
                   WebAssembly.CompileError,
                   /segment's element type must be subtype of table's element type/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (func $f)
    (elem nullref (ref.func $f) (ref.null) (ref.null)))`)),
                   WebAssembly.CompileError,
                   /initializer type must be subtype of element type/);

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (elem nullref (ref.null) (ref.null) (ref.null))
    (func (export "f")
      (table.init $t 0 (i32.const 0) (i32.const 0) (i32.const 2))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (elem anyref (ref.null) (ref.null) (ref.null))
    (func (export "f")
      (table.init $t 0 (i32.const 0) (i32.const 0) (i32.const 2))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected nullref/);

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (func (export "f") (result nullref)
      (table.get $t (i32.const 0)))
    (func (export "g") (result anyref)
      (table.get $t (i32.const 0)))
    (func (export "h") (param $p nullref)
      (table.set $t (i32.const 0) (local.get $p))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 anyref)
    (func (export "f") (result nullref)
      (table.get $t (i32.const 0))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected nullref/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (func (export "f") (param $p anyref)
      (table.set $t (i32.const 0) (local.get $p))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected nullref/);

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (table $u 10 nullref)
    (func (export "f")
      (table.copy $u $t (i32.const 0) (i32.const 0) (i32.const 2))))`));

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (table $u 10 anyref)
    (func (export "f")
      (table.copy $u $t (i32.const 0) (i32.const 0) (i32.const 2))))`));

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 nullref)
    (table $u 10 funcref)
    (func (export "f")
      (table.copy $u $t (i32.const 0) (i32.const 0) (i32.const 2))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $t 10 anyref)
    (table $u 10 nullref)
    (func (export "f")
      (table.copy $u $t (i32.const 0) (i32.const 0) (i32.const 2))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected nullref/);

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $u 10 nullref)
    (func (export "f")
      (table.fill $u (i32.const 0) (ref.null) (i32.const 2))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $u 10 nullref)
    (func (export "f") (param $r anyref)
      (table.fill $u (i32.const 0) (local.get $r) (i32.const 2))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected nullref/);

new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $u 10 nullref)
    (func (export "f")
      (table.grow $u (ref.null) (i32.const 2))
      (drop)))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (table $u 10 nullref)
    (func (export "f") (param $r anyref)
      (table.grow $u (local.get $r) (i32.const 2))
      (drop)))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected nullref/);

{
    let tab = new WebAssembly.Table({element:"nullref", initial:10});
    assertEq(tab.get(0), null);
    tab.set(5, null);
    assertErrorMessage(() => tab.set(5, 0), TypeError, /nullref requires a null value/);
    assertErrorMessage(() => tab.set(5, undefined), TypeError, /nullref requires a null value/);
    effect = false;
    assertErrorMessage(() => tab.set(5, effectful), TypeError, /nullref requires a null value/);
    assertEq(effect, false);

    assertEq(tab.grow(5, null), 10);
    assertEq(tab.length, 15);
    assertErrorMessage(() => tab.grow(1, "abracadabra"), TypeError, /nullref requires a null value/);
}

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (import "m" "t" (table 10 nullref)))`)),
                         {m:{t:new WebAssembly.Table({element:"nullref", initial:10})}});

{
    let ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (table (export "t") 10 nullref))`)));
    new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "t" (table 10 nullref)))`)),
                             {m:ins1.exports});
}

{
    let ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (table (export "t") 10 anyref))`)));
    assertErrorMessage(() => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "t" (table 10 nullref)))`)),
                                                      {m:ins1.exports}),
                       WebAssembly.LinkError,
                       /imported table type mismatch/);
}

// Globals
new WebAssembly.Module(wasmTextToBinary(`
  (module
    (global nullref (ref.null)))`))

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
  (module
    (func (export "f"))
    (global nullref (ref.func 0)))`)),
                   WebAssembly.CompileError,
                   /initializer type and expected type don't match/);

new WebAssembly.Global({ value: "nullref", mutable: true });

assertEq(new WebAssembly.Global({ value: "nullref", mutable: true }, null).value,
         null);

effect = false;
assertErrorMessage(() => new WebAssembly.Global({ value: "nullref", mutable: true }, effectful),
                   TypeError,
                   /nullref requires a null value/);
assertEq(effect, false);

{
    let ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (global (export "g") (mut nullref) (ref.null)))`)));
    new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "g" (global (mut nullref))))`)),
                             {m:ins1.exports});
}

{
    let ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (global (export "g") (mut anyref) (ref.null)))`)));
    assertErrorMessage(() => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "g" (global (mut nullref))))`)),
                                                      {m:ins1.exports}),
                       WebAssembly.LinkError,
                       /imported global type mismatch/);
}

{
    let ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (global (export "g") (mut nullref) (ref.null)))`)));
    assertErrorMessage(() => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "m" "g" (global (mut anyref))))`)),
                                                      {m:ins1.exports}),
                       WebAssembly.LinkError,
                       /imported global type mismatch/);
}
