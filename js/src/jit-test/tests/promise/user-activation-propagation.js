function newPromiseCapability() {
    return {};
}
function neverCalled() {}
function resolveCapability(dIs) {}
class P extends Promise {
    constructor(executor) {
        executor(resolveCapability, neverCalled);
        var p = async function() {}();
        p.constructor = {
            [Symbol.species]: P
        };
        return p;
    }
}
var {
    promise: alwaysPending
} = newPromiseCapability();
P.race([alwaysPending]).then(neverCalled, neverCalled);
