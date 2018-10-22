// |jit-test| module

let result = null;
let error = null;
let promise = import("nonexistent.js");
promise.then((ns) => {
    result = ns;
}).catch((e) => {
    error = e;
});

drainJobQueue();
assertEq(result, null);
assertEq(error instanceof Error, true);
