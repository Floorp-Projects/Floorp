// Bug 972961 - Test compiler with very long chains of property accesses

// Return true if we can compile a chain of n property accesses,
// false if we cannot. Throw if compilation fails in an unexpected way.
function test(n) {
    print("testing " + n);
    try {
        eval('if (false) {' + Array(n).join(("a.")) + 'a}');
    } catch (exc) {
        // Expected outcome if the expression is too deeply nested is an InternalError.
        if (!(exc instanceof InternalError))
            throw exc;
        print(exc.message);
        return false;
    }
    print("no exception");
    return true;
}

// Find out how long a chain is enough to break the compiler.
var n = 4, LIMIT = 0x000fffff;
var lo = 1, hi = 1;
while (n <= LIMIT && test(n)) {
    lo = n;
    n *= 4;
}

// Using binary search, find a pass/fail boundary (in order to
// test the edge case).
if (n <= LIMIT) {
    hi = n;
    while (lo !== hi) {
        var mid = Math.floor((lo + hi) / 2);
        if (test(mid))
            lo = mid + 1;
        else
            hi = mid;
    }
    print((lo - 1) + " attributes should be enough for anyone");
}
