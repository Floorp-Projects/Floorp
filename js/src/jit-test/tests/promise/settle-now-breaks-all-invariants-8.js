// Don't assert when settlePromiseNow() is called on an async-function promise.

var promise = async function(){ await 0; }();

try {
    settlePromiseNow(promise);
} catch {}
