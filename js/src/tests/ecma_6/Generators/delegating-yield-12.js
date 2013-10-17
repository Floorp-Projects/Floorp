// yield* calls @@iterator on the iterable to produce the iterator.

var log = '';

function IteratorWrapper(iterator) {
    return {
        next: function (val) {
            log += 'n';
            return iterator.next(val);
        },

        throw: function (exn) {
            log += 't';
            return iterator.throw(exn);
        }
    };
}

function IterableWrapper(iterable) {
    var ret = {};

    ret[std_iterator] = function () {
        log += 'i';
        return IteratorWrapper(iterable[std_iterator]());
    }

    return ret;
}

function* delegate(iter) { return yield* iter; }

var iter = delegate(IterableWrapper([1, 2, 3]));
assertIteratorResult(iter.next(), 1, false);
assertIteratorResult(iter.next(), 2, false);
assertIteratorResult(iter.next(), 3, false);
assertIteratorResult(iter.next(), undefined, true);

assertEq(log, 'innnn');

iter = delegate([1, 2, 3]);
assertIteratorResult(iter.next(), 1, false);
assertIteratorResult(iter.next(), 2, false);
assertIteratorResult(iter.next(), 3, false);
assertIteratorResult(iter.next(), undefined, true);

assertEq(log, 'innnn');

if (typeof reportCompare == "function")
    reportCompare(true, true);
