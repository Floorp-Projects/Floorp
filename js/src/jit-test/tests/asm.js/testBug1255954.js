// |jit-test| slow; skip-if: !('oomTest' in this)

const USE_ASM = '"use asm";';
function asmCompile() {
    var f = Function.apply(null, arguments);
}
oomTest(() => {
    try {
        function f(b) {}
    } catch (exc0) {}
    f(asmCompile(USE_ASM + "function f() { var i=42; return i|0; for(;1;) {} return 0 } return f"));
});
