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

var {promise, resolve} = newPromiseCapability();

var getterCount = 0;

class P extends Promise {
    constructor(executor) {
        var {promise, resolve, reject} = newPromiseCapability();

        executor(function(v) {
            // Resolve the promise.
            resolve(v);

            // But then return an object from the resolve function. This object
            // must be treated as the resolution value for the otherwise
            // skipped promise which gets created when Promise.prototype.then is
            // called in PerformPromiseRace.
            return {
                get then() {
                    getterCount++;
                }
            };
        }, neverCalled);

        return promise;
    }

    // Default to the standard Promise.resolve function, so we don't create
    // another instance of this class when resolving the passed promise objects
    // in Promise.race.
    static resolve(v) {
        return Promise.resolve(v);
    }
}

P.race([promise]);

resolve(0);

drainJobQueue();

assertEq(getterCount, 1);
