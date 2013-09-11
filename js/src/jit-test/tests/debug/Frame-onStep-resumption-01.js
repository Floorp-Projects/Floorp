// If frame.onStep returns {return:val}, the frame returns.

var g = newGlobal();
g.eval("function f(x) {\n" +
       "    var a = x * x;\n" +
       "    return a;\n" +
       "}\n");

var dbg = Debugger(g);
dbg.onEnterFrame = function (frame) {
    frame.onStep = function () { return {return: "pass"}; };
};

assertEq(g.f(4), "pass");
