// Test we don't assert when the promise is settled after enqueuing a PromiseReactionJob.

function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}


var {promise, resolve} = newPromiseCapability();

var p = Promise.resolve(0);

// Enqueue a PromiseResolveThenableJob followed by a PromiseReactionJob.
resolve(p);

// The PromiseReactionJob expects a pending promise, but this settlePromiseNow
// call will already have settled the promise.
settlePromiseNow(promise);
