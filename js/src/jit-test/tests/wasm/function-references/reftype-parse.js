// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

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
    
// RefType/ValueType can be specified as an {ref: 'func', ...} object
const t11 = new WebAssembly.Table({element: {ref: 'func', nullable: true}, initial: 3});
const t12 = new WebAssembly.Table({element: {ref: 'extern', nullable: true}, initial: 3});
const t13 = new WebAssembly.Table({element: {ref: 'extern', nullable: false}, initial: 3}, {});

assertErrorMessage(
    () => new WebAssembly.Table({element: {ref: 'func', nullable: false}, initial: 1}, null),
    TypeError, /cannot pass null to non-nullable WebAssembly reference/);
assertErrorMessage(
    () => new WebAssembly.Table({element: {ref: 'extern', nullable: false}, initial: 1}, null),
    TypeError, /cannot pass null to non-nullable WebAssembly reference/);

assertErrorMessage(
    () => new WebAssembly.Table({element: {ref: 'bar', nullable: true}, initial: 1}),
    TypeError, /bad value type/);

const g11 = new WebAssembly.Global({value: {ref: 'func', nullable: true}, mutable: true});
const g12 = new WebAssembly.Global({value: {ref: 'extern', nullable: true}, mutable: true});
const g13 = new WebAssembly.Global({value: {ref: 'extern', nullable: false}, mutable: true}, {});
const g14 = new WebAssembly.Global({value: {ref: 'extern', nullable: false}, mutable: true});
const g15 = new WebAssembly.Global({value: {ref: 'extern', nullable: false}, mutable: true}, void 0);

assertErrorMessage(
    () => new WebAssembly.Global({value: {ref: 'func', nullable: false}, mutable: true}),
    TypeError, /cannot pass null to non-nullable WebAssembly reference/);
assertErrorMessage(
    () => new WebAssembly.Global({value: {ref: 'extern', nullable: false}, mutable: true}, null),
    TypeError, /cannot pass null to non-nullable WebAssembly reference/);
    
assertErrorMessage(
    () => new WebAssembly.Global({value: {ref: 'bar', nullable: true}, mutable: true}),
    TypeError, /bad value type/);
