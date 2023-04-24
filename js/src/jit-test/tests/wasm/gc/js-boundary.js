// |jit-test| skip-if: !wasmGcEnabled()

// Tests of dynamic type checks
test('anyref', WasmAnyrefValues, WasmNonAnyrefValues);
test('eqref', WasmEqrefValues, WasmNonAnyrefValues);
test('structref', WasmStructrefValues, WasmNonAnyrefValues);
test('arrayref', WasmArrayrefValues, WasmNonAnyrefValues);
let { newStruct } = wasmEvalText(`
  (module
    (type $s (struct))
    (func (export "newStruct") (result anyref)
        struct.new $s)
  )`).exports;
test('(ref null 0)', [newStruct()], WasmNonAnyrefValues, '(type (struct))');
test('nullref', [null], WasmNonAnyrefValues);

function test(type, validValues, invalidValues, typeSection) {
  const CheckError = /can only pass|bad type/;

  if (!typeSection) {
    typeSection = "";
  }

  // 1. Exported function params
  let {a} = wasmEvalText(`(module
    ${typeSection}
    (func (export "a") (param ${type}))
  )`).exports;
  for (let val of invalidValues) {
    assertErrorMessage(() => a(val), TypeError, CheckError);
  }
  for (let val of validValues) {
    a(val);
  }

  // 2. Imported function results
  for (let val of invalidValues) {
    function returnVal() {
      return val;
    }
    let {test} = wasmEvalText(`(module
      ${typeSection}
      (func $returnVal (import "" "returnVal") (result ${type}))
      (func (export "test")
        call $returnVal
        drop
      )
    )`, {"": {returnVal}}).exports;
    assertErrorMessage(() => test(), TypeError, CheckError);
  }
  for (let val of validValues) {
    function returnVal() {
      return val;
    }
    let {test} = wasmEvalText(`(module
      ${typeSection}
      (func $returnVal (import "" "returnVal") (result ${type}))
      (func (export "test")
        call $returnVal
        drop
      )
    )`, {"": {returnVal}}).exports;
    test(val);
  }

  // TODO: the rest of the tests cannot handle type sections yet.
  if (typeSection !== "") {
    return;
  }

  // 3. Global value setter
  for (let val of validValues) {
    // Construct global from JS-API with initial value
    let a = new WebAssembly.Global({value: type}, val);
    assertEq(a.value, val, 'roundtrip matches');

    // Construct global from JS-API with null value, then set
    let b = new WebAssembly.Global({value: type, mutable: true}, null);
    b.value = val;
    assertEq(b.value, val, 'roundtrip matches');
  }
  for (let val of invalidValues) {
    // Construct global from JS-API with initial value
    assertErrorMessage(() => new WebAssembly.Global({value: type}, val),
      TypeError,
      CheckError);

    // Construct global from JS-API with null value, then set
    let a = new WebAssembly.Global({value: type, mutable: true}, null);
    assertErrorMessage(() => a.value = val,
      TypeError,
      CheckError);
  }

  // 4. Table set method
  for (let val of validValues) {
    let table = new WebAssembly.Table({element: type, initial: 1, maximum: 1});
    table.set(0, val);
    assertEq(table.get(0), val, 'roundtrip matches');
  }
  for (let val of invalidValues) {
    let table = new WebAssembly.Table({element: type, initial: 1, maximum: 1});
    assertErrorMessage(() => table.set(0, val),
      TypeError,
      CheckError);
  }
}
