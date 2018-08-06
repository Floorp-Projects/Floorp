function* g(iter) {
    yield* iter;
}

var calledReturn = false;

var it = g({
    [Symbol.iterator]() {
        return this;
    },
    next() {
        return {done: false};
    },
    throw: createIsHTMLDDA(),
    return() {
        calledReturn = true;
        return {done: false};
    }
});

it.next();

assertThrowsInstanceOf(() => it.throw(""), TypeError);

assertEq(calledReturn, false);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
