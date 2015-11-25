// |jit-test| --dump-bytecode
if (!('oomTest' in this))
    quit();

function f() {
    eval("(function() {})()");
}
oomTest(f);
