// |jit-test| skip-if: !('oomTest' in this)

function f() {
  // Too many results returned.
  return [52, 10, 0, 0];
}

let binary = wasmTextToBinary(`
  (module
    (import "env" "f" (func $f (result i32 i32 i32)))
    (func (export "run") (result i32)
      (call $f)
      i32.sub
      i32.sub))
`);

let module = new WebAssembly.Module(binary);
let instance = new WebAssembly.Instance(module, { env: { f } });
let run = instance.exports.run;

// Run once for setup.
try { run(); } catch {}

oomTest(run);
