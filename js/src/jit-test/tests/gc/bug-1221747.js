// |jit-test| --dump-bytecode

function f() {
    eval("(function() {})()");
}
oomTest(f);
