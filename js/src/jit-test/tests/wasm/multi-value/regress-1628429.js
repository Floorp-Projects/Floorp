let bytes = wasmTextToBinary(`
   (module
     (func $f (import "imports" "f") (param i32 i32) (result i32 i32)))`);

new WebAssembly.Instance(new WebAssembly.Module(bytes),
                         { 'imports': { 'f': Uint16Array } });
