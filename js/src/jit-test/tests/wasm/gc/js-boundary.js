// |jit-test| skip-if: !wasmGcEnabled()

// Tests of dynamic type checks
test('anyref', WasmAnyrefValues, []);
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

function test(type, validValues, invalidValues, typeSection = "") {
  const CheckError = /can only pass|bad type/;

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

// Verify that GC objects are opaque
for (const val of WasmGcObjectValues) {
  assertEq(Reflect.getPrototypeOf(val), null);
  assertEq(Reflect.setPrototypeOf(val, null), true);
  assertEq(Reflect.setPrototypeOf(val, {}), false);
  assertEq(Reflect.isExtensible(val), false);
  assertEq(Reflect.preventExtensions(val), false);
  assertEq(Reflect.getOwnPropertyDescriptor(val, "anything"), undefined);
  assertEq(Reflect.defineProperty(val, "anything", { value: 42 }), false);
  assertEq(Reflect.has(val, "anything"), false);
  assertEq(Reflect.get(val, "anything"), undefined);
  assertErrorMessage(() => { Reflect.set(val, "anything", 3); }, TypeError, /can't modify/);
  assertErrorMessage(() => { Reflect.deleteProperty(val, "anything"); }, TypeError, /can't modify/);
  assertEq(Reflect.ownKeys(val).length, 0, `gc objects should not have keys, but this one had: ${Reflect.ownKeys(val)}`);
  for (const i in val) {
    throw new Error(`GC objects should have no enumerable properties, but had ${i}`);
  }
  assertEq(val[Symbol.iterator], undefined, "GC objects should not be iterable");
}
