var g = newGlobal({newCompartment: true});
g.parent = this;
g.evaluate(`var dbg = new Debugger(parent); dbg.onEnterFrame = function(){};`);

function f() {
    for (var i = 0; i < 10; i++) {
        var arr = [1, 2, , i];
        assertEq(2 in arr, false);
        assertEq(3 in arr, true);
    }
}
f();
