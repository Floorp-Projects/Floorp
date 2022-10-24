// |reftest| shell-option(--enable-array-from-async) skip-if(!Array.fromAsync)

// Basic Smoke Test
async function* asyncGen(n) {
    for (let i = 0; i < n; i++) {
        yield i * 2;
    }
}

let done = false;
Array.fromAsync(asyncGen(4)).then((x) => {
    assertEq(Array.isArray(x), true);
    assertEq(x.length, 4);
    assertEq(x[0], 0);
    assertEq(x[1], 2);
    assertEq(x[2], 4);
    assertEq(x[3], 6);
    done = true;
}
);

drainJobQueue();
assertEq(done, true);

(async function () {
    class InterruptableAsyncIterator {
        count = 0
        closed = false
        throwAfter = NaN
        constructor(n, throwAfter = NaN) {
            this.count = n;
            this.throwAfter = throwAfter;
        }
        [Symbol.asyncIterator] = function () {
            return {
                iter: this,
                i: 0,
                async next() {
                    if (this.i > this.iter.throwAfter) {
                        throw "Exception"
                    }
                    if (this.i++ < this.iter.count) {
                        return Promise.resolve({ done: false, value: this.i - 1 });
                    }
                    return Promise.resolve({ done: true, value: undefined });
                },
                async return(x) {
                    this.iter.closed = true;
                    return { value: x, done: true };
                }
            }
        }
    }

    var one = await Array.fromAsync(new InterruptableAsyncIterator(2));
    assertEq(one.length, 2)
    assertEq(one[0], 0);
    assertEq(one[1], 1);

    var two = new InterruptableAsyncIterator(10, 2);
    var threw = false;
    try {
        var res = await Array.fromAsync(two);
    } catch (e) {
        threw = true;
        assertEq(e, "Exception");
    }
    assertEq(threw, true);
    // The iterator is not closed unless we have an abrupt completion while mapping.
    assertEq(two.closed, false);

    // Test throwing while mapping: Iterator should be closed.
    var three = new InterruptableAsyncIterator(10, 9);
    threw = false;
    try {
        var res = await Array.fromAsync(three, (x) => {
            if (x > 3) {
                throw "Range"
            }
            return x;
        });
    } catch (e) {
        assertEq(e, "Range");
        threw = true;
    }
    assertEq(threw, true);
    assertEq(three.closed, true);

    var sync = await Array.fromAsync([1, 2, 3]);
    assertEq(sync.length, 3);
    assertEq(sync[0], 1)
    assertEq(sync[1], 2)
    assertEq(sync[2], 3)

    let closed_frozen = false;
    class Frozen {
        constructor(x) {
            this.count = x;
            Object.freeze(this);
        }
        [Symbol.asyncIterator] = function () {
            return {
                iter: this,
                i: 0,
                async next() {
                    if (this.i++ < this.iter.count) {
                        return Promise.resolve({ done: false, value: this.i - 1 });
                    }
                    return Promise.resolve({ done: true, value: undefined });
                },
                async return(x) {
                    // Can't use Frozen instance, becuse frozen is frozen.
                    closed_frozen = true;
                    return { value: x, done: true };
                }
            }
        }
    }

    // We should close the iterator when define property throws.
    // Test by defining into a frozen object. 
    var frozen = new Frozen(10);
    threw = false;
    try {
        var result = await Array.fromAsync.call(Frozen, frozen);
    } catch (e) {
        threw = true;
    }

    assertEq(threw, true);
    assertEq(closed_frozen, true);

})();

drainJobQueue();
if (typeof reportCompare === 'function')
    reportCompare(true, true);