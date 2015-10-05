var Promise = ShellPromise;

var oldThen = Promise.prototype.then;

// Changing then() should not break catch()
Promise.prototype.then = function() { throw new Error(); };

new Promise(a => { throw new Error(); })
  .catch(() => {
    if (typeof reportCompare === "function")
        reportCompare(true, true);
  });
