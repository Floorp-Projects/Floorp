// |jit-test| --dump-bytecode; skip-if: !('oomTest' in this)

function f() {
    eval("(function() {})()");
}
oomTest(f);
