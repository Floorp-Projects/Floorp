// |jit-test| allow-oom

function f() {
    eval("(function() y)()");
}
oomTest(f);
fullcompartmentchecks(true);
