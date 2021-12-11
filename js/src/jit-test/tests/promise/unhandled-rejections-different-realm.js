// |jit-test| error:Unhandled rejection: "some reason"

// Test JS shell's unhandled rejection tracking.

var z = newGlobal();

Promise.prototype.then = z.Promise.prototype.then;

// Add unhandled rejection from other realm.
evalcx("var p = (async function() { throw 'some reason' })()", z);

// Add unhandled rejection from this realm.
var p = (async function f() { throw 'other reason'; })();

// Remove unhandled rejection from this realm.
p.then();

// Remove unhandled rejection from other realm.
evalcx("p.then()", z);
