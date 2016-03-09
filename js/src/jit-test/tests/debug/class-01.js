// |jit-test| error: TypeError

let g = newGlobal();
let dbg = Debugger(g);

let forceException = g.eval(`
    (class extends class {} {
        // Calling this will throw for using |this| uninitialized.
        constructor() { }
    })
`);

dbg.onExceptionUnwind = function() {
    return {
        // Force the return of an illegal value.
        return: 1
    }
}

new forceException;
