// Don't assert when a side-effect when getting the "then" property settled the promise.

function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}


var {promise, resolve} = newPromiseCapability();

var thenable = {
    get then() {
        settlePromiseNow(promise);

        // Throw an error to reject the promise.
        throw new Error();
    }
};

resolve(thenable);
