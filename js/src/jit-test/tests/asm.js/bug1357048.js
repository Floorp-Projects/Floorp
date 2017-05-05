// |jit-test|
load(libdir + "asm.js");

function runTest() {
    var code = USE_ASM + `
        function f() {
            var x = 0;
            var y = 0;
            x = (((0x77777777 - 0xcccccccc) | 0) % -1) | 1;
            y = (((0x7FFFFFFF + 0x7FFFFFFF) | 0) % -1) | 0;
            return (x + y) | 0;
        }
        return f;
    `;

    assertEq(asmLink(asmCompile(code))(), 1);
}

runTest();
setARMHwCapFlags('vfp');
runTest();
