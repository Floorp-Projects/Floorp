load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);

g.eval(`
async function f() {
    return e;
}
`);

// Just continue
dbg.onExceptionUnwind = function(frame) {
    return undefined;
};
g.eval(`
var E;
f().catch(e => { exc = e });
drainJobQueue();
assertEq(exc instanceof ReferenceError, true);
`);

// Return with resumption value.
dbg.onExceptionUnwind = function(frame) {
    return {
        return: 10
    };
};
var val = g.eval(`
var val;
f().then(v => { val = v });
drainJobQueue();
val;
`);
assertEq(val, 10);
