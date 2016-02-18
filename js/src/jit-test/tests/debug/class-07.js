// |jit-test| error: ReferenceError

let g = newGlobal();
let dbg = Debugger(g);

let forceException = g.eval(`
    (class extends class {} {
        // Calling this will return a primitive immediately.
        constructor() { return {}; }
    })
`);

dbg.onEnterFrame = function() {
    return {
        // Force the return undefined, which will throw for returning
        // while |this| is still undefined.
        return: undefined
    }
}
print("break here");
new forceException;
