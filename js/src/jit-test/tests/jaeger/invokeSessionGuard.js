var g = newGlobal();
var dbg = new g.Debugger(this);

[1,2,3,4,5,6,7,8].forEach(
    function(x) {
        // evalInFrame means lightweight gets call obj
        assertEq(evalInFrame(0, "x"), x);
    }
);
