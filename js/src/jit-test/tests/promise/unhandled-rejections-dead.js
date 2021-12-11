// |jit-test| error:Unhandled rejection

// Create the set object for unhandled rejection in this global.
async function fn() { e }
let p = fn();

var g = newGlobal();
g.evaluate(`
async function fn() { e }
fn()
// Create unhandled rejection in another compartment.
// The promise is tracked by the unhandled rejection set with CCW.
P = newGlobal().eval("(class extends Promise { function(){} })")

// Nuke the CCW to make the entry in unhandled rejection set a dead proxy.
Promise.all.call(P, [{ then() { nukeAllCCWs() } }])
`);
