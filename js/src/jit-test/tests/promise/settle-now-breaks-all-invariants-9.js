function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}


var {promise, resolve, reject} = newPromiseCapability();

settlePromiseNow(promise);

assertEq(resolve(0), undefined);
assertEq(reject(0), undefined);
