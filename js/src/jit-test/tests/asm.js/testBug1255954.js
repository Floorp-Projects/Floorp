const USE_ASM = '"use asm";';
if (!('oomTest' in this))
    quit();
function asmCompile() {
    var f = Function.apply(null, arguments);
}
oomTest(() => {
    try {
        function f(b) {}
    } catch (exc0) {}
    f(asmCompile(USE_ASM + "function f() { var i=42; return i|0; for(;1;) {} return 0 } return f"));
});
