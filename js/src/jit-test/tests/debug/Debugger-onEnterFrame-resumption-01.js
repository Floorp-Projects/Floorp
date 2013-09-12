// If debugger.onEnterFrame returns {return:val}, the frame returns.

var g = newGlobal();
g.set = false;
g.eval("function f() {\n" +
       "    set = true;\n" +
       "    return 'fail';\n" +
       "}\n");
g.eval("function g() { return 'g ' + f(); }");
g.eval("function h() { return 'h ' + g(); }");

var dbg = Debugger(g);
var savedFrame;
dbg.onEnterFrame = function (frame) {
    savedFrame = frame;
    return {return: "pass"};
};

// Call g.f as a function.
savedFrame = undefined;
assertEq(g.f(), "pass");
assertEq(savedFrame.live, false);
assertEq(g.set, false);

// Call g.f as a constructor.
savedFrame = undefined;
var r = new g.f;
assertEq(typeof r, "object");
assertEq(savedFrame.live, false);
assertEq(g.set, false);

var count = 0;
dbg.onEnterFrame = function (frame) {
    count++;
    if (count == 3) {
        savedFrame = frame;
        return {return: "pass"};
    }
    return undefined;
};
g.set = false;
savedFrame = undefined;
assertEq(g.h(), "h g pass");
assertEq(savedFrame.live, false);
assertEq(g.set, false);
