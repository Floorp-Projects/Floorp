// |jit-test| allow-oom; skip-if: !('oomTest' in this)

function f() {
    eval("(function() y)()");
}
oomTest(f);
fullcompartmentchecks(true);
