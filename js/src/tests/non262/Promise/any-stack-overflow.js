// Bug 1646317 - Don't assert on stack overflow under Promise.any().

if ("ignoreUnhandledRejections" in this) {
  ignoreUnhandledRejections();
}

Array.prototype[Symbol.iterator] = function*() {
  let rejected = Promise.reject(0);
  let p = Promise.any([rejected]);
}
new Set(Object.keys(this));
new Set(Object.keys(this));

this.reportCompare && reportCompare(true, true);
