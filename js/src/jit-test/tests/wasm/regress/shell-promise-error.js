// |jit-test| error:done

// Make a biggish module that will take a bit of time to compile.
var code = '(module ';
for (var i = 0; i < 1000; i++) {
    code += '(func (local $i i32) ';
    for (var j = 0; j < 100; j++)
        code += '(set_local $i (i32.const 42))';
    code += ')';
}
code += ')';

WebAssembly.compile(wasmTextToBinary(code));

throw "done";
