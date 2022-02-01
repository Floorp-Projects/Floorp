// |jit-test| skip-if: !wasmSimdEnabled()

// This should not release-assert, which it could previously do on some 32-bit
// platforms due to the too-limited size of a bitfield.

const MaxParams = 1000;         // Per spec

var params = '';
for ( var i=0 ; i < MaxParams-1; i++ ) {
    params += '(param v128) '
}
params += '(param externref)'

new WebAssembly.Module(wasmTextToBinary(`
(module
  (func $f)
  (func ${params} (result externref)
    (call $f)
    (local.get ${MaxParams-1})))`));
