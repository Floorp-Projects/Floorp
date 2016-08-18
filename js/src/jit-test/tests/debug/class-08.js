let g = newGlobal();
let dbg = Debugger(g);
dbg.onDebuggerStatement = function() {
    // Force the constructor to return undefined, which should be replaced with
    // |this| if the latter has been initialized.
    return { return: undefined };
}

assertEq(g.eval(`
    new (class extends class {} {
        constructor() { super(); this.foo = 42; debugger; }
    })
`).foo, 42);
