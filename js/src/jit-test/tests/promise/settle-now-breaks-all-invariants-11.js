// Don't assert when the promise in the resolving functions is wrapped in a CCW.

function newPromiseCapability(newTarget) {
    var resolve, reject, promise = Reflect.construct(Promise, [function(r1, r2) {
        resolve = r1;
        reject = r2;
    }], newTarget);
    return {promise, resolve, reject};
}

var g = newGlobal();

var {promise, reject} = newPromiseCapability(g.Promise);

g.settlePromiseNow(promise);

// Don't assert when rejecting the promise.
reject(0);
