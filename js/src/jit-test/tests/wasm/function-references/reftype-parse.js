// |jit-test| skip-if: !wasmGcEnabled()

// RefType/ValueType as a simple string
const t01 = new WebAssembly.Table({element: 'funcref', initial: 3});
const t02 = new WebAssembly.Table({element: 'externref', initial: 8});
const g01 = new WebAssembly.Global({value: 'funcref', mutable: true}, null);
const g02 = new WebAssembly.Global({value: 'externref', mutable: true}, null);

// Specify ToString() equivalents
const t05 = new WebAssembly.Table({element: {toString() { return 'funcref' },}, initial: 11});
const t06 = new WebAssembly.Table({element: ['externref'], initial: 7});

assertErrorMessage(
    () => new WebAssembly.Table({element: 'foo', initial: 1}),
    TypeError, /bad value type/);
assertErrorMessage(
    () => new WebAssembly.Table({element: true, initial: 1}),
    TypeError, /bad value type/);
assertErrorMessage(
    () => new WebAssembly.Global({value: {ref: 'bar', nullable: true}, mutable: true}),
    TypeError, /bad value type/);
