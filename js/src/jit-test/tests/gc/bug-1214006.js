// |jit-test| allow-oom

if (!('oomTest' in this))
    quit();

function f() {
    eval("(function() y)()");
}
oomTest(f);
fullcompartmentchecks(true);
