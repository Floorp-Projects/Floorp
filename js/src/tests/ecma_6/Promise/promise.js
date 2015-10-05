/**
 * Because there is no way to "wait" for a callback in this testing system,
 * there was a need to make one big promise out of all test cases.
 */

var Promise = ShellPromise;

Promise.all([

  assertEventuallyEq(new Promise(resolve => resolve(2)), 2),

  assertEventuallyThrows(new Promise((_, reject) => reject(new Error())), Error),

  assertEventuallyThrows(new Promise(() => { throw new Error(); }), Error),

  assertEventuallyEq(new Promise(resolve => resolve())
    .then(() => 3), 3),

  assertEventuallyEq(new Promise(resolve => resolve())
    .then(() => new Promise(r => r(3))), 3),

  assertEventuallyEq(new Promise((_, reject) => reject(new Error()))
    .catch(() => new Promise(r => r(3))), 3),

  assertEventuallyThrows(new Promise(resolve => resolve())
    .then(() => { throw new Error(); }), Error),

  assertEventuallyEq(new Promise((_, reject) => reject(new Error()))
    .catch(() => 4), 4),

  assertEventuallyEq(Promise.resolve(5), 5),

  assertEventuallyThrows(Promise.reject(new Error()), Error),

  assertEventuallyDeepEq(Promise.all([]), []),

  assertEventuallyDeepEq(Promise.all(Array(10).fill()
    .map((_, id) => new Promise(resolve => resolve(id)))),
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]),

])
    .then(() => {
      if (typeof reportCompare === "function")
          reportCompare(true, true);
        });
