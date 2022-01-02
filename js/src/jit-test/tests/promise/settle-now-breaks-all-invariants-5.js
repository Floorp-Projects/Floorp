// Test we don't assert when the promise is settled and we then try to call the
// rejecting function.

function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}


var {promise, reject} = newPromiseCapability();

settlePromiseNow(promise);

// Don't assert when the promise is already settled.
reject(0);
