load(libdir + "asm.js");
load(libdir + "asserts.js");

var fatFunc = USE_ASM + '\n';
// Recursion depth reduced to allow PBL with debug build (hence larger
// frames) to work.
for (var i = 0; i < 75; i++)
    fatFunc += "function f" + i + "() { return ((f" + (i+1) + "()|0)+1)|0 }\n";
fatFunc += "function f75() { return 42 }\n";
fatFunc += "return f0";

for (let threshold of [0, 50, 100, 5000, -1]) {
    setJitCompilerOption("jump-threshold", threshold);

    assertEq(asmCompile(
        USE_ASM + `
            function h() { return ((g()|0)+2)|0 }
            function g() { return ((f()|0)+1)|0 }
            function f() { return 42 }
            return h
        `)()(), 45);

    enableGeckoProfiling();
    asmLink(asmCompile(USE_ASM + 'function f() {} function g() { f() } function h() { g() } return h'))();
    disableGeckoProfiling();

    assertEq(asmCompile(fatFunc)()(), 117);
}
