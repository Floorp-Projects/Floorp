// |jit-test| skip-if: !wasmGcEnabled()

// Tests of eqref dynamic type checks
const EqrefCheckError = /can only pass a TypedObject to an eqref/;

// 1. Exported function params

let {a} = wasmEvalText(`(module
  (func (export "a") (param eqref))
)`).exports;
for (let val of WasmNonEqrefValues) {
  assertErrorMessage(() => a(val), TypeError, EqrefCheckError);
}
for (let val of WasmEqrefValues) {
  a(val);
}

// 2. Imported function results

for (let val of WasmNonEqrefValues) {
  function returnVal() {
    return val;
  }
  let {test} = wasmEvalText(`(module
    (func $returnVal (import "" "returnVal") (result eqref))
    (func (export "test")
      call $returnVal
      drop
    )
  )`, {"": {returnVal}}).exports;
  assertErrorMessage(() => test(), TypeError, EqrefCheckError);
}
for (let val of WasmEqrefValues) {
  function returnVal() {
    return val;
  }
  let {test} = wasmEvalText(`(module
    (func $returnVal (import "" "returnVal") (result eqref))
    (func (export "test")
      call $returnVal
      drop
    )
  )`, {"": {returnVal}}).exports;
  test(val);
}

// 3. Global value setter

for (let val of WasmEqrefValues) {
  // Construct global from JS-API with initial value
  let a = new WebAssembly.Global({value: 'eqref'}, val);
  assertEq(a.value, val, 'roundtrip matches');

  // Construct global from JS-API with null value, then set
  let b = new WebAssembly.Global({value: 'eqref', mutable: true}, null);
  b.value = val;
  assertEq(b.value, val, 'roundtrip matches');
}
for (let val of WasmNonEqrefValues) {
  // Construct global from JS-API with initial value
  assertErrorMessage(() => new WebAssembly.Global({value: 'eqref'}, val),
    TypeError,
    EqrefCheckError);

  // Construct global from JS-API with null value, then set
  let a = new WebAssembly.Global({value: 'eqref', mutable: true}, null);
  assertErrorMessage(() => a.value = val,
    TypeError,
    EqrefCheckError);
}

// 4. Table set method

for (let val of WasmEqrefValues) {
  let table = new WebAssembly.Table({element: 'eqref', initial: 1, maximum: 1});
  table.set(0, val);
  assertEq(table.get(0), val, 'roundtrip matches');
}
for (let val of WasmNonEqrefValues) {
  let table = new WebAssembly.Table({element: 'eqref', initial: 1, maximum: 1});
  assertErrorMessage(() => table.set(0, val),
    TypeError,
    EqrefCheckError);
}
