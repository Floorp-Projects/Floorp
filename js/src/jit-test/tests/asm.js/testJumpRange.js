load(libdir + "asm.js");
load(libdir + "asserts.js");

var fatFunc = USE_ASM + '\n';
for (var i = 0; i < 100; i++)
    fatFunc += "function f" + i + "() { return ((f" + (i+1) + "()|0)+1)|0 }\n";
fatFunc += "function f100() { return 42 }\n";
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

    assertEq(asmCompile(fatFunc)()(), 142);
}
