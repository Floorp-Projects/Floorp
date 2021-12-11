// Test that jobs added to the promise job queue have a global that's consistent
// with and without same-compartment realms.

var g1 = newGlobal();
var g2 = newGlobal({sameCompartmentAs: this});

// EnqueuePromiseReactionJob, handler is a primitive.
// Job's global is the reaction's global.
function test1(g) {
    var resolve;
    var p = new Promise(res => { resolve = res; });
    g.Promise.prototype.then.call(p, 1);
    resolve();
    assertEq(globalOfFirstJobInQueue(), g);
    drainJobQueue();
}
test1(g1);
test1(g2);

// EnqueuePromiseReactionJob, handler is an object.
// Job's global is the handler's global.
function test2(g) {
    var resolve;
    var p = new Promise(res => { resolve = res; });
    p.then(new g.Function());
    resolve();
    assertEq(globalOfFirstJobInQueue(), g);
    drainJobQueue();
}
test2(g1);
test2(g2);
