// |jit-test| error:dead object
var g = newGlobal({newCompartment: true});
var p = new Promise(() => {});
g.Promise.prototype.then.call(p, () => void 0);
g.eval("nukeAllCCWs()");
resolvePromise(p, 9);
