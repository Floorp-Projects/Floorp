// The first call to yield* passes one arg to "next".

function Iter() {
    function next() {
        if (arguments.length != 1)
            throw Error;
        return { value: 42, done: true }
    }

    this.next = next;
    this[Symbol.iterator] = function () { return this; }
}

function* delegate(iter) { return yield* iter; }

var iter = delegate(new Iter());
assertDeepEq(iter.next(), {value:42, done:true});

if (typeof reportCompare == "function")
    reportCompare(true, true);
