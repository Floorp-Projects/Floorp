// |reftest| skip-if(!xulRuntime.shell)

var test = `

class base {
    constructor() {
        eval('');
        (()=>0)();
    }
}

class derived extends base {
    constructor() {
        eval('');
    }
}

// Make sure eval and arrows are still valid in non-derived constructors.
new base();


// Eval throws in derived class constructors, in both class expressions and
// statements.
assertThrowsInstanceOf((() => new derived()), InternalError);
assertThrowsInstanceOf((() => new class extends base { constructor() { eval('') } }()), InternalError);

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function(frame) { assertThrowsInstanceOf(()=>frame.eval(''), InternalError); };

g.eval("new class foo extends null { constructor() { debugger; return {}; } }()");
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
