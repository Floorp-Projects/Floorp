// Promise.race(...) may add a dummy PromiseReaction which is only used for the
// debugger. Ensure that this dummy reaction can't influence the normal Promise
// resolution behaviour.
//
// See BlockOnPromise when called from PerformPromiseRace for when this dummy
// reaction is created.

function newPromiseCapability() {
    var resolve, reject, promise = new Promise(function(r1, r2) {
        resolve = r1;
        reject = r2;
    });
    return {promise, resolve, reject};
}

function neverCalled() {
    // Quit with non-zero exit code to ensure a test suite error is shown,
    // even when this function is called within promise handlers which normally
    // swallow any exceptions.
    quit(1);
}

var c = 0;
var g_resolve;

var resolvedValues = [];

function resolveCapability(v) {
   resolvedValues.push(v);
}

class P extends Promise {
    constructor(executor) {
        // Only the very first object created through this constructor gets
        // special treatment, all other invocations create built-in Promise
        // objects.
        if (c++ > 1) {
            return new Promise(executor);
        }

        executor(resolveCapability, neverCalled);

        var {promise, resolve} = newPromiseCapability();
        g_resolve = resolve;

        // Use an async function to create a Promise without resolving functions.
        var p = async function(){ await promise; return 456; }();

        // Ensure the species constructor is not the built-in Promise constructor
        // to avoid falling into the fast path.
        p.constructor = {
            [Symbol.species]: P
        };

        return p;
    }
}

var {promise: alwaysPending} = newPromiseCapability();

// The promise returned from race() should never be resolved.
P.race([alwaysPending]).then(neverCalled, neverCalled);

g_resolve(123);

drainJobQueue();

// Check |resolvedValues| to ensure resolving functions were properly called.
assertEq(resolvedValues.length, 2);
assertEq(resolvedValues[0], alwaysPending);
assertEq(resolvedValues[1], 456);
