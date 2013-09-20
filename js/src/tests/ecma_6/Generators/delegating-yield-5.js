// Test that a deep yield* chain re-yields received results without
// re-boxing.

function results(results) {
    var i = 0;
    function next() {
        return results[i++];
    }
    return { next: next }
}

function* yield_results(expected, n) {
    return yield* n ? yield_results(expected, n - 1) : results(expected);
}

function collect_results(iter) {
    var ret = [];
    var result;
    do {
        result = iter.next();
        ret.push(result);
    } while (!result.done);
    return ret;
}

// We have to put a full result for the end, because the return will re-box.
var expected = [{value: 1}, 13, "foo", {value: 34, done: true}];

assertDeepEq(expected, collect_results(results(expected)));
assertDeepEq(expected, collect_results(yield_results(expected, 20)));

if (typeof reportCompare == "function")
    reportCompare(true, true);
