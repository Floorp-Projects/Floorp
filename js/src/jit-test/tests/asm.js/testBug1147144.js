// |jit-test| skip-if: !isAsmJSCompilationAvailable()

function test() {
load(libdir + 'asm.js');
load(libdir + 'asserts.js');

assertEq(eval(`(function asmModule() { "use asm"; function asmFun() {} return asmFun })`).toString(), "function asmModule() {\n    [native code]\n}");
assertEq(eval(`(function asmModule() { "use asm"; function asmFun() {} return asmFun })`)().toString(), "function asmFun() {\n    [native code]\n}");
assertEq((evaluate(`function asmModule1() { "use asm"; function asmFun() {} return asmFun }`), asmModule1).toString(), "function asmModule1() {\n    [native code]\n}");
assertEq((evaluate(`function asmModule3() { "use asm"; function asmFun() {} return asmFun }`), asmModule3)().toString(), "function asmFun() {\n    [native code]\n}");
assertEq(asmCompile(USE_ASM + `function asmFun() {} return asmFun`).toString(), "function anonymous() {\n    [native code]\n}");
assertEq(asmCompile(USE_ASM + `function asmFun() {} return asmFun`)().toString(), "function asmFun() {\n    [native code]\n}");
assertThrowsInstanceOf(()=>asmCompile('stdlib',USE_ASM + "var sin=stdlib.Math.sin;return {}")({Math:{}}), Error);
}

var g = newGlobal({ discardSource: true });
g.libdir = libdir;
g.evaluate(test.toString() + "test()");
