// |jit-test| skip-if: !wasmGcEnabled()

// implicit null funcref value
{
  const { t, get } = wasmEvalText(`(module
    (table (export "t") 3 funcref)
    (func (export "get") (param i32) (result funcref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), null);
  assertEq(t.get(1), null);
  assertEq(t.get(2), null);
  assertEq(get(0), null);
  assertEq(get(1), null);
  assertEq(get(2), null);
}

// explicit null funcref value
{
  const { t, get } = wasmEvalText(`(module
    (table (export "t") 3 funcref (ref.null func))
    (func (export "get") (param i32) (result funcref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), null);
  assertEq(t.get(1), null);
  assertEq(t.get(2), null);
  assertEq(get(0), null);
  assertEq(get(1), null);
  assertEq(get(2), null);
}

// actual funcref value
{
  const { t, get, foo } = wasmEvalText(`(module
    (func $foo (export "foo"))

    (table (export "t") 3 funcref (ref.func $foo))
    (func (export "get") (param i32) (result funcref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), foo);
  assertEq(t.get(1), foo);
  assertEq(t.get(2), foo);
  assertEq(get(0), foo);
  assertEq(get(1), foo);
  assertEq(get(2), foo);
}

// implicit null anyref value
{
  const { t, get } = wasmEvalText(`(module
    (table (export "t") 3 anyref)
    (func (export "get") (param i32) (result anyref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), null);
  assertEq(t.get(1), null);
  assertEq(t.get(2), null);
  assertEq(get(0), null);
  assertEq(get(1), null);
  assertEq(get(2), null);
}

// explicit null anyref value
{
  const { t, get } = wasmEvalText(`(module
    (table (export "t") 3 anyref (ref.null any))
    (func (export "get") (param i32) (result anyref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), null);
  assertEq(t.get(1), null);
  assertEq(t.get(2), null);
  assertEq(get(0), null);
  assertEq(get(1), null);
  assertEq(get(2), null);
}

// actual anyref value
{
  const { t, get } = wasmEvalText(`(module
    (type $s (struct))

    (table (export "t") 3 anyref (struct.new $s))
    (func (export "get") (param i32) (result anyref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(!!t.get(0), true);
  assertEq(!!t.get(1), true);
  assertEq(!!t.get(2), true);
  assertEq(!!get(0), true);
  assertEq(!!get(1), true);
  assertEq(!!get(2), true);
}

// implicit null externref value
{
  const { t, get } = wasmEvalText(`(module
    (table (export "t") 3 externref)
    (func (export "get") (param i32) (result externref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), null);
  assertEq(t.get(1), null);
  assertEq(t.get(2), null);
  assertEq(get(0), null);
  assertEq(get(1), null);
  assertEq(get(2), null);
}

// explicit null externref value
{
  const { t, get } = wasmEvalText(`(module
    (table (export "t") 3 externref (ref.null extern))
    (func (export "get") (param i32) (result externref)
      (table.get (local.get 0))
    )
  )`).exports;

  assertEq(t.get(0), null);
  assertEq(t.get(1), null);
  assertEq(t.get(2), null);
  assertEq(get(0), null);
  assertEq(get(1), null);
  assertEq(get(2), null);
}

// actual externref value (from an imported global, which is visible to tables)
{
  const foo = "wowzers";
  const { t, get } = wasmEvalText(`(module
    (global (import "" "foo") externref)

    (table (export "t") 3 externref (global.get 0))
    (func (export "get") (param i32) (result externref)
      (table.get (local.get 0))
    )
  )`, { "": { "foo": foo } }).exports;

  assertEq(t.get(0), foo);
  assertEq(t.get(1), foo);
  assertEq(t.get(2), foo);
  assertEq(get(0), foo);
  assertEq(get(1), foo);
  assertEq(get(2), foo);
}

// non-imported globals come after tables and are therefore not visible
assertErrorMessage(() => wasmEvalText(`(module
  (global anyref (ref.null any))
  (table 3 anyref (global.get 0))
)`), WebAssembly.CompileError, /global.get index out of range/);
