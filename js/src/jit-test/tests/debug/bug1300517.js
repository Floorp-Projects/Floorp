// |jit-test| error: ReferenceError
g = newGlobal();
g.log *= "";
Debugger(g).onDebuggerStatement = frame => frame.eval("log += this.Math.toString();");
let forceException = g.eval(`
    (class extends class {} {
        constructor() {
            debugger;
        }
    })
`);
new forceException;
