function newPromiseCapability() {
    let resolve, reject, promise = new Promise(function(r1, r2) {
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

class P extends Promise {
    constructor(executor) {
        // Only the very first object created through this constructor gets
        // special treatment, all other invocations create built-in Promise
        // objects.
        if (c++ > 1) {
            return new Promise(executor);
        }

        // Pass a native ResolvePromiseFunction function as the resolve handler.
        // (It's okay that the promise of this promise capability is never used.)
        executor(newPromiseCapability().resolve, neverCalled);

        let {promise, resolve} = newPromiseCapability();
        g_resolve = resolve;

        // Use an async function to create a Promise without resolving functions.
        return async function(){ await promise; return 456; }();
    }

    // Ensure we don't take the (spec) fast path in Promise.resolve and instead
    // create a new promise object. (We could not provide an override at all
    // and rely on the default behaviour, but giving an explicit definition
    // may help to interpret this test case.)
    static resolve(v) {
        return super.resolve(v);
    }
}

let {promise: alwaysPending} = newPromiseCapability();

P.race([alwaysPending]).then(neverCalled, neverCalled);

g_resolve(123);

drainJobQueue();
