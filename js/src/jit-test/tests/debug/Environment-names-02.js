// env.names() on object environments ignores property names that are not identifiers.

var g = newGlobal();
var dbg = Debugger(g);
var names;
g.h = function () {
    names = dbg.getNewestFrame().environment.names();
};
g.eval("var obj = {a: 1};\n" +
       "with ({a: 1, '0xcafe': 2, ' ': 3, '': 4, '0': 5}) h();");
assertEq(names.indexOf("a") !== -1, true);
assertEq(names.indexOf("0xcafe"), -1);
assertEq(names.indexOf(" "), -1);
assertEq(names.indexOf(""), -1);
assertEq(names.indexOf("0"), -1);
