
// This exposed a bug in CL's aarch64 isel, in which the -4098 was
// incorrectly sign-extended out to 64 bits.

let { exports } = new WebAssembly.Instance(new WebAssembly.Module(
    wasmTextToBinary(`
      (module
        (type (;0;) (func))
        (type (;1;) (func (result i32)))
        (type (;2;) (func (result i64)))
        (func (;0;) (type 0)
          i32.const -4098
          i32.load16_s offset=1
          drop)
        (memory (;0;) 1)
        (export "g" (func 0)))` )));
try {
    exports.g();
} catch (e) {}

