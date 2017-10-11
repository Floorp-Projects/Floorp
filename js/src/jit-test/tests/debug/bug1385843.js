var g = newGlobal();
g.parent = this;
g.count = 0;
g.eval("(" + function() {
    var dbg = new Debugger(parent);
    dbg.onEnterFrame = function(frame) {
        if (count === 5)
            dbg.onEnterFrame = undefined;
        count++;
        var ex = frame.eval("this").throw.unsafeDereference();
        assertEq(ex.message.includes("uninitialized"), true);
        assertEq(ex.message.includes("Foo2"), true);
    }
} + ")()");
class Foo1 {};
class Foo2 extends Foo1 {
    constructor() {
        super();
    }
};
new Foo2();
assertEq(g.count, 6);
