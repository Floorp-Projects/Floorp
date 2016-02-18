// |jit-test| error: TypeError

let g = newGlobal();
let dbg = Debugger(g);

let forceException = g.eval(`
    (class extends class {} {
        // Calling this will return a primitive immediately.
        constructor() { return {}; }
    })
`);

dbg.onEnterFrame = function(f) {
    f.onStep = function() {
        return {
            // Force the return of an illegal value.
            return: 1
        }
    }
}

new forceException;
