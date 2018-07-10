// Promise.race(...) may add a dummy PromiseReaction which is only used for the
// debugger.
//
// See BlockOnPromise when called from PerformPromiseRace for when this dummy
// reaction is created.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

function test(withFastPath) {
    g.eval(`
        function newPromiseCapability() {
            var resolve, reject, promise = new Promise(function(r1, r2) {
                resolve = r1;
                reject = r2;
            });
            return {promise, resolve, reject};
        }

        var {promise: alwaysPending} = newPromiseCapability();

        if (!${withFastPath}) {
            // Disable the BlockOnPromise fast path by giving |alwaysPending| a
            // non-default "then" function property. This will ensure the dummy
            // reaction is created.
            alwaysPending.then = function() {};
        }

        var result = Promise.race([alwaysPending]);
    `);

    var alwaysPending = gw.makeDebuggeeValue(g.alwaysPending);
    var result = gw.makeDebuggeeValue(g.result);

    assertEq(alwaysPending.promiseDependentPromises.length, 1);
    assertEq(alwaysPending.promiseDependentPromises[0], result);

    assertEq(result.promiseDependentPromises.length, 0);
}

// No dummy reaction created when the fast path is taken.
test(true);

// Dummy reaction is created when we can't take the fast path.
test(false);
