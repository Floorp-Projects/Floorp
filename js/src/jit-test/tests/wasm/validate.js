const { validate } = WebAssembly;

assertErrorMessage(() => validate(), Error, /At least 1 argument required/);

const argError = /first argument must be an ArrayBuffer or typed array object/;
assertErrorMessage(() => validate(null), Error, argError);
assertErrorMessage(() => validate(true), Error, argError);
assertErrorMessage(() => validate(42), Error, argError);
assertErrorMessage(() => validate(NaN), Error, argError);
assertErrorMessage(() => validate('yo'), Error, argError);
assertErrorMessage(() => validate([]), Error, argError);
assertErrorMessage(() => validate({}), Error, argError);
assertErrorMessage(() => validate(Symbol.iterator), Error, argError);
assertErrorMessage(() => validate({ valueOf: () => new ArrayBuffer(65536) }), Error, argError);

assertEq(validate(wasmTextToBinary(`(module)`)), true);

assertEq(validate(wasmTextToBinary(`(module (export "run" (func 0)))`)), false);
assertEq(validate(wasmTextToBinary(`(module (func) (export "run" (func 0)))`)), true);

// Feature-testing proof-of-concept.
assertEq(validate(wasmTextToBinary(`(module (memory 1) (func (result i32) (memory.size)))`)), true);
assertEq(validate(wasmTextToBinary(`(module (memory 1) (func (result i32) (memory.grow (i32.const 42))))`)), true);
