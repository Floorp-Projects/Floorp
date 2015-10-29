if (!('oomAfterAllocations' in this))
    quit();
gcslice(0); // Start IGC, but don't mark anything.
function f(str) {
    for (var i = 0; i < 10; i++) {
        arr = /foo(ba(r))?/.exec(str);
        var x = arr[oomAfterAllocations(100)] + " " + arr[1] + " " + 1899;
    }
}
try {
    f("foo");
} catch(e) {}
