// Test changes to the WebAssembly.Table API by reference-types

function assertTableFilled(table, length, element) {
  assertEq(table.length, length);
  for (let i = 0; i < length; i++) {
    assertEq(table.get(i), element);
  }
}

const tableLength = 10;

// Test table constructor can accept a default value to fill elements to
for (let value of WasmExternrefValues) {
  let table = new WebAssembly.Table({'element': 'externref', initial: tableLength}, value);
  assertTableFilled(table, tableLength, value);
}
for (let value of WasmFuncrefValues) {
  let table = new WebAssembly.Table({'element': 'funcref', initial: tableLength}, value);
  assertTableFilled(table, tableLength, value);
}

// Test not specifying default value in constructor yields 'undefined' for externref
{
  let table = new WebAssembly.Table({'element': 'externref', initial: tableLength});
  assertTableFilled(table, tableLength, undefined);
}

// Test not specifying default value in constructor yields 'null' for funcref
{
  let table = new WebAssembly.Table({'element': 'funcref', initial: tableLength});
  assertTableFilled(table, tableLength, null);
}

// Test omitting the value in table set defaults to undefined for externref and
// null for funcref.
{
    let t = new WebAssembly.Table({element:"externref", initial: 1});
    // Clear out initial value
    t.set(0, '');
    // Set with an omitted value
    t.set(0);
    // Assert the omitted value is undefined
    assertEq(t.get(0), undefined);
}
{
    let t = new WebAssembly.Table({element:"funcref", initial: 1});
    // Clear out initial value
    t.set(0, WasmFuncrefValues[0]);
    // Set with an omitted value
    t.set(0);
    // Assert the omitted value is null
    assertEq(t.get(0), null);
}

// Test table grow. There is an optional fill argument that defaults to
// undefined with externref, and null with funcref.
{
    let t = new WebAssembly.Table({element:"externref", initial:0});
    t.grow(1);
    assertEq(t.get(t.length-1), undefined);
    let prev = undefined;
    for (let v of WasmExternrefValues) {
        t.grow(2, v);
        assertEq(t.get(t.length-3), prev);
        assertEq(t.get(t.length-2), v);

        assertEq(t.get(t.length-1), v);
        prev = v;
    }
}

{
    let t = new WebAssembly.Table({element:"funcref", initial:0});
    t.grow(1);
    assertEq(t.get(t.length-1), null);
    let prev = null;
    for (let v of WasmFuncrefValues) {
        t.grow(2, v);
        assertEq(t.get(t.length-3), prev);
        assertEq(t.get(t.length-2), v);

        assertEq(t.get(t.length-1), v);
        prev = v;
    }
}

// If growing by zero elements there are no spurious writes
{
    let t = new WebAssembly.Table({element:"externref", initial:1});
    t.set(0, 1337);
    t.grow(0, 1789);
    assertEq(t.get(0), 1337);
}
