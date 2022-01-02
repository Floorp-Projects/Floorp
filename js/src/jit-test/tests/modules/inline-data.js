// |jit-test| module

import { a } from "javascript: export let a = 42;";
assertEq(a, 42);

let result = null;
let error = null;
let promise = import("javascript: export let b = 100;");
promise.then((ns) => {
    result = ns;
}).catch((e) => {
    error = e;
});

drainJobQueue();
assertEq(error, null);
assertEq(result.b, 100);
