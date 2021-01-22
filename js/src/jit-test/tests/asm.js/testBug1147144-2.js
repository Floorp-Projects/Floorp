// |jit-test| skip-if: !isAsmJSCompilationAvailable()||!Function.prototype.toSource

function test() {
load(libdir + 'asm.js');
load(libdir + 'asserts.js');

assertEq(eval(`(function asmModule() { "use asm"; function asmFun() {} return asmFun })`).toSource(), "(function asmModule() {\n    [native code]\n})");
assertEq(eval(`(function asmModule() { "use asm"; function asmFun() {} return asmFun })`)().toSource(), "function asmFun() {\n    [native code]\n}");
assertEq((evaluate(`function asmModule2() { "use asm"; function asmFun() {} return asmFun }`), asmModule2).toSource(), "function asmModule2() {\n    [native code]\n}");
assertEq((evaluate(`function asmModule4() { "use asm"; function asmFun() {} return asmFun }`), asmModule4)().toSource(), "function asmFun() {\n    [native code]\n}");
assertEq(asmCompile(USE_ASM + `function asmFun() {} return asmFun`).toSource(), "(function anonymous() {\n    [native code]\n})");
assertEq(asmCompile(USE_ASM + `function asmFun() {} return asmFun`)().toSource(), "function asmFun() {\n    [native code]\n}");
}

var g = newGlobal({ discardSource: true });
g.libdir = libdir;
g.evaluate(test.toString() + "test()");
