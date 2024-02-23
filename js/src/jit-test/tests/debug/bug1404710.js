stackTest(new Function(`
    var g = newGlobal();
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function (frame) {
        frame.evalWithBindings("x", {x: 2}).return;
    };
    g.eval("function f(y) { debugger; }");
    g.f(3);
`), false);
