// |jit-test| error:Unhandled rejection

load(libdir + "asserts.js");

// Calling settlePromiseNow on already-resolved promise should throw, and
// unhandled rejection tracking should work.

assertThrowsInstanceOf(() => {
  var promise = new Promise(resolve => {
    resolve(10);
  });
  settlePromiseNow(promise);
}, Error);


assertThrowsInstanceOf(() => {
  var promise = new Promise((_, reject) => {
    reject(10);
  });
  settlePromiseNow(promise);
}, Error);

assertThrowsInstanceOf(() => {
  var promise = new Promise(() => {
    throw 10;
  });
  settlePromiseNow(promise);
}, Error);
