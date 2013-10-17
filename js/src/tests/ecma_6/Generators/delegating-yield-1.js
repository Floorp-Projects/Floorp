// This file was written by Andy Wingo <wingo@igalia.com> and originally
// contributed to V8 as generators-objects.js, available here:
//
// http://code.google.com/p/v8/source/browse/branches/bleeding_edge/test/mjsunit/harmony/generators-objects.js

// Test that yield* re-yields received results without re-boxing.

function results(results) {
    var i = 0;
    function next() {
        return results[i++];
    }
    var iter = { next: next }
    var ret = {};
    ret[std_iterator] = function () { return iter; }
    return ret;
}

function* yield_results(expected) {
    return yield* results(expected);
}

function collect_results(iterable) {
    var ret = [];
    var result;
    var iter = iterable[std_iterator]();
    do {
        result = iter.next();
        ret.push(result);
    } while (!result.done);
    return ret;
}

// We have to put a full result for the end, because the return will re-box.
var expected = [{value: 1}, 13, "foo", {value: 34, done: true}];

// Sanity check.
assertDeepEq(expected, collect_results(results(expected)));
assertDeepEq(expected, collect_results(yield_results(expected)));

if (typeof reportCompare == "function")
    reportCompare(true, true);
