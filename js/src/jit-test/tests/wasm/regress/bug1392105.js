// |jit-test| --arm-asm-nop-fill=1

var code = "(module ";
for (var i = 0; i < 100; i++)
  code += "(func (param i32) (result i32) (i32.add (i32.const 1) (get_local 0))) ";
code += ")";
var buf = wasmTextToBinary(code);
WebAssembly.compile(buf);
