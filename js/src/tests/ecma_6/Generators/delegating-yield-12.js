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
assertIteratorResult(1, false, iter.next());
assertIteratorResult(2, false, iter.next());
assertIteratorResult(3, false, iter.next());
assertIteratorResult(undefined, true, iter.next());

assertEq(log, 'innnn');

iter = delegate([1, 2, 3]);
assertIteratorResult(1, false, iter.next());
assertIteratorResult(2, false, iter.next());
assertIteratorResult(3, false, iter.next());
assertIteratorResult(undefined, true, iter.next());

assertEq(log, 'innnn');

if (typeof reportCompare == "function")
    reportCompare(true, true);
