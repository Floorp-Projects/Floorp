// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

var BUGNUMBER = 1180306;
var summary = 'Promise.{all,race} should close iterator on error';

print(BUGNUMBER + ": " + summary);

function test(ctor, props, { nextVal=undefined,
                             nextThrowVal=undefined,
                             modifier=undefined,
                             rejectReason=undefined,
                             rejectType=undefined,
                             closed=true }) {
    function getIterable() {
        let iterable = {
            closed: false,
            [Symbol.iterator]() {
                let iterator = {
                    first: true,
                    next() {
                        if (this.first) {
                            this.first = false;
                            if (nextThrowVal)
                                throw nextThrowVal;
                            return nextVal;
                        }
                        return { value: undefined, done: true };
                    },
                    return() {
                        iterable.closed = true;
                        return {};
                    }
                };
                if (modifier)
                    modifier(iterator, iterable);

                return iterator;
            }
        };
        return iterable;
    }
    for (let prop of props) {
        let iterable = getIterable();
        let e;
        ctor[prop](iterable).catch(e_ => { e = e_; });
        drainJobQueue();
        if(rejectType)
            assertEq(e instanceof rejectType, true);
        else
            assertEq(e, rejectReason);
        assertEq(iterable.closed, closed);
    }
}

// == Error cases with close ==

// ES 2017 draft 25.4.4.1.1 step 6.i.
// ES 2017 draft 25.4.4.3.1 step 3.h.
class MyPromiseStaticResolveGetterThrows extends Promise {
    static get resolve() {
        throw "static resolve getter throws";
    }
};
test(MyPromiseStaticResolveGetterThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    rejectReason: "static resolve getter throws",
    closed: true,
});

class MyPromiseStaticResolveThrows extends Promise {
    static resolve() {
        throw "static resolve throws";
    }
};
test(MyPromiseStaticResolveThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    rejectReason: "static resolve throws",
    closed: true,
});

// ES 2017 draft 25.4.4.1.1 step 6.q.
// ES 2017 draft 25.4.4.3.1 step 3.i.
class MyPromiseThenGetterThrows extends Promise {
    static resolve() {
        return {
            get then() {
                throw "then getter throws";
            }
        };
    }
};
test(MyPromiseThenGetterThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    rejectReason: "then getter throws",
    closed: true,
});

class MyPromiseThenThrows extends Promise {
    static resolve() {
        return {
            then() {
                throw "then throws";
            }
        };
    }
};
test(MyPromiseThenThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    rejectReason: "then throws",
    closed: true,
});

// ES 2017 draft 7.4.6 step 3.
// if GetMethod fails, the thrown value should be used.
test(MyPromiseThenThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    modifier: (iterator, iterable) => {
        Object.defineProperty(iterator, "return", {
            get: function() {
                iterable.closed = true;
                throw "return getter throws";
            }
        });
    },
    rejectReason: "return getter throws",
    closed: true,
});
test(MyPromiseThenThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    modifier: (iterator, iterable) => {
        Object.defineProperty(iterator, "return", {
            get: function() {
                iterable.closed = true;
                return "non object";
            }
        });
    },
    rejectType: TypeError,
    closed: true,
});
test(MyPromiseThenThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    modifier: (iterator, iterable) => {
        Object.defineProperty(iterator, "return", {
            get: function() {
                iterable.closed = true;
                // Non callable.
                return {};
            }
        });
    },
    rejectType: TypeError,
    closed: true,
});

// ES 2017 draft 7.4.6 steps 6.
// if return method throws, the thrown value should be ignored.
test(MyPromiseThenThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    modifier: (iterator, iterable) => {
        iterator.return = function() {
            iterable.closed = true;
            throw "return throws";
        };
    },
    rejectReason: "then throws",
    closed: true,
});

test(MyPromiseThenThrows, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    modifier: (iterator, iterable) => {
        iterator.return = function() {
            iterable.closed = true;
            return "non object";
        };
    },
    rejectReason: "then throws",
    closed: true,
});

// == Error cases without close ==

// ES 2017 draft 25.4.4.1.1 step 6.a.
test(Promise, ["all", "race"], {
    nextThrowVal: "next throws",
    rejectReason: "next throws",
    closed: false,
});

test(Promise, ["all", "race"], {
    nextVal: { value: {}, get done() { throw "done getter throws"; } },
    rejectReason: "done getter throws",
    closed: false,
});

// ES 2017 draft 25.4.4.1.1 step 6.e.
test(Promise, ["all", "race"], {
    nextVal: { get value() { throw "value getter throws"; }, done: false },
    rejectReason: "value getter throws",
    closed: false,
});

// ES 2017 draft 25.4.4.1.1 step 6.d.iii.2.
let first = true;
class MyPromiseResolveThrows extends Promise {
    constructor(executer) {
        if (first) {
            first = false;
            super((resolve, reject) => {
                executer(() => {
                    throw "resolve throws";
                }, reject);
            });
            return;
        }
        super(executer);
    }
};
test(MyPromiseResolveThrows, ["all"], {
    nextVal: { value: undefined, done: true },
    rejectReason: "resolve throws",
    closed: false,
});

// == Successful cases ==

test(Promise, ["all", "race"], {
    nextVal: { value: Promise.resolve(1), done: false },
    closed: false,
});

if (typeof reportCompare === 'function')
  reportCompare(true, true);
