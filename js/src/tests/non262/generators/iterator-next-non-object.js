var BUGNUMBER = 1016936;
var summary = "IteratorNext should throw if the value returned by iterator.next() is not an object.";

print(BUGNUMBER + ": " + summary);

var nonobjs = [
    null,
    undefined,
    1,
    true,
    "a",
    Symbol.iterator,
];

function createIterable(v) {
    var iterable = {};
    iterable[Symbol.iterator] = function () {
        return {
            next: function () {
                return v;
            }
        };
    };
    return iterable;
}

function f() {
}

for (var nonobj of nonobjs) {
    var iterable = createIterable(nonobj);

    assertThrowsInstanceOf(() => [...iterable], TypeError);
    assertThrowsInstanceOf(() => f(...iterable), TypeError);

    assertThrowsInstanceOf(() => { for (var x of iterable) {} }, TypeError);

    assertThrowsInstanceOf(() => {
        var [a] = iterable;
    }, TypeError);
    assertThrowsInstanceOf(() => {
        var [...a] = iterable;
    }, TypeError);

    assertThrowsInstanceOf(() => Array.from(iterable), TypeError);
    assertThrowsInstanceOf(() => new Map(iterable), TypeError);
    assertThrowsInstanceOf(() => new Set(iterable), TypeError);
    assertThrowsInstanceOf(() => new WeakMap(iterable), TypeError);
    assertThrowsInstanceOf(() => new WeakSet(iterable), TypeError);
    // FIXME: bug 1232266
    // assertThrowsInstanceOf(() => new Int8Array(iterable), TypeError);
    assertThrowsInstanceOf(() => Int8Array.from(iterable), TypeError);

    assertThrowsInstanceOf(() => {
        var g = function*() {
            yield* iterable;
        };
        var v = g();
        v.next();
    }, TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
