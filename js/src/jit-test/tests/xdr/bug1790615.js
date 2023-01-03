// |jit-test| --fuzzing-safe; --cpu-count=2; --ion-offthread-compile=off; skip-if: !isAsmJSCompilationAvailable()

load(libdir + "asserts.js");

function asmCompile() {
  var f = Function.apply(null, arguments);
  return f;
}
var code = asmCompile('glob', 'imp', 'b', `
"use asm";
function f(i,j) {
  i=i|0;
  j=j|0;
}
return f
`);
let g80 = newGlobal({newCompartment: true});
// Compiling to a Stencil XDR should fail because XDR encoding does not support asm.js
assertThrowsInstanceOf(() => g80.compileToStencilXDR(code, {}), g80.Error);