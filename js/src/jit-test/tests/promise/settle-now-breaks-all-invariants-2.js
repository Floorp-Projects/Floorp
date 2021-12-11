// Test we don't assert when the promise is settled and the SpeciesConstructor
// call in Promise.prototype.then throws an exception.

function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}


var {promise, resolve} = newPromiseCapability();

var p = Promise.resolve(0);

p.constructor = {
    [Symbol.species]: function() {
        throw new Error();
    }
};

// Enqueue a PromiseResolveThenableJob.
resolve(p);

// Settle the promise after the resolve call.
settlePromiseNow(promise);
