// env.find() finds nonenumerable names in the global environment.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
g.h = function () {
    var env = dbg.getNewestFrame().environment;
    var last = env;
    while (last.parent)
        last = last.parent;

    assertEq(env.find("Array"), last);
    hits++;
};

g.eval("h();");
g.eval("(function () { let (x = 1, y = 2) h(); })();");
assertEq(hits, 2);
