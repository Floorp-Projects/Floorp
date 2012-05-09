var g = newGlobal('new-compartment');
g.eval("var success = false");
g.eval("function ponies() {}");
g.eval("function foo() { ponies(); success = false }");

var dbg = new Debugger(g);
dbg.onEnterFrame = function(frame) {
    // The goal here is force an early return on the 'call' instruction,
    // which should be the 3rd step (callgname, undefined, call)
    var step = 0;
    frame.onStep = function() {
        ++step;
        if (step == 2) {
            g.success = true;
            return;
        }
        if (step == 3)
            return { return: undefined }
    }
    frame.onPop = function() { new Error(); /* boom */ }
}

g.foo();
assertEq(g.success, true);
