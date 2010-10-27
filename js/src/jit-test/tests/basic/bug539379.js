try {
    // On X64 this was crashing rather than causing a "too much recursion" exception.
    x = /x/;
    (function f() {
        x.r = x;
        return f()
    })();
} catch (e) {}

