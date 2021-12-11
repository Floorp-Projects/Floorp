// The iteratee of yield* can be a proxy.

function results(results) {
    var i = 0;
    function iterator() {
        return this;
    }
    function next() {
        return results[i++];
    }
    var ret = { next: next }
    ret[Symbol.iterator] = iterator;
    return ret;
}

function* yield_results(expected) {
    return yield* new Proxy(results(expected), {});
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
var expected = [{value: 1}, {value: 34, done: true}];

// Sanity check.
assertDeepEq(expected, collect_results(results(expected)));
assertDeepEq(expected, collect_results(yield_results(expected)));

if (typeof reportCompare == "function")
    reportCompare(true, true);
