// |jit-test| exitstatus: 6; skip-if: helperThreadCount() == 0

runOffThreadScript(offThreadCompileScript(`
    function asmModule() {
        "use asm";
        function f() {
            while(1) {
            }
        }
        return f;
    }
`));
timeout(1);
asmModule()();
assertEq(true, false);
