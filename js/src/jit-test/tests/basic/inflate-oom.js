// |jit-test| skip-if: !('oomTest' in this)
function test() {
    function foo() {
        return 1;
    };
    oomTest(() => {
        gc();
        foo.toString();
    });
}
var s = ";".repeat(70000);
s += test.toString() + "test()";
s += ";".repeat(70000);
eval(s);
