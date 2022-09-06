// |jit-test| skip-if: helperThreadCount() === 0

// Debugger.allowUnobservedAsmJS with off-thread parsing.

load(libdir + "asm.js");


var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("dbg = new Debugger(parent);");

assertEq(g.dbg.allowUnobservedAsmJS, false);

enableLastWarning();

var asmFunStr = USE_ASM + 'function f() {} return f';
offThreadCompileToStencil("(function() {" + asmFunStr + "})");
var stencil = finishOffThreadStencil();
evalStencil(stencil);

var msg = getLastWarning().message;
assertEq(msg === "asm.js type error: Asm.js optimizer disabled by debugger" ||
         msg === "asm.js type error: Asm.js optimizer disabled because no suitable wasm compiler is available" ||
         msg === "asm.js type error: Asm.js optimizer disabled by 'asmjs' runtime option" ||
         msg === "asm.js type error: Asm.js optimizer disabled because the compiler is disabled or unavailable",
         true);
