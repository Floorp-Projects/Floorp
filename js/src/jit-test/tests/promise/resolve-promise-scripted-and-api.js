function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}


var {promise, resolve} = newPromiseCapability();

resolve(Promise.resolve(0));

// Don't assert when the Promise was already resolved.
resolvePromise(promise, 123);
