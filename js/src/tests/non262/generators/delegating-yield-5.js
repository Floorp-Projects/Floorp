// Test that a deep yield* chain re-yields received results without
// re-boxing.

function results(results) {
    var i = 0;
    function next() {
        return results[i++];
    }
    var iter = { next: next };
    var ret = {};
    ret[Symbol.iterator] = function () { return iter; }
    return ret;
}

function* yield_results(expected, n) {
    return yield* n ? yield_results(expected, n - 1) : results(expected);
}

function collect_results(iterable) {
    var ret = [];
    var result;
    var iter = iterable[Symbol.iterator]();
    do {
        result = iter.next();
        ret.push(result);
    } while (!result.done);
    return ret;
}

// We have to put a full result for the end, because the return will re-box.
var expected = [{value: 1}, {value: 34, done: true}];

assertDeepEq(expected, collect_results(results(expected)));
assertDeepEq(expected, collect_results(yield_results(expected, 20)));

if (typeof reportCompare == "function")
    reportCompare(true, true);
