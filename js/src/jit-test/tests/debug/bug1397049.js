// Run debugger in its own global
let g = newGlobal({newCompartment: true});
g.target = this;
g.evaluate(`
    let d = new Debugger;
    let gw = d.addDebuggee(target);

    d.onDebuggerStatement = function(frame)
    {
        frame = frame.older;

        let res = frame.eval("this");
        assertEq(res.return, frame.this);

        res = frame.evalWithBindings("this", {x:42});
        assertEq(res.return, frame.this);
    }
`);

// Debugger statement affects parse so hide in another function
function brk() { debugger; }

function f1() {
    var temp = "string";
    brk();
}

function f2() {
    let temp = "string";
    brk();
}

function f3() {
    const temp = "string";
    brk();
}

f1.call({});
f2.call({});
f3.call({});
