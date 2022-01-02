// env.names() on object environments ignores property names that are not identifiers.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var withNames, globalNames;
g.h = function () {
    var env = dbg.getNewestFrame().environment;
    withNames = env.names();
    while (env.parent !== null)
        env = env.parent;
    globalNames = env.names();
};

g.eval("" +
       function fill(obj) {
           obj.sanityCheck = 1;
           obj["0xcafe"] = 2;
           obj[" "] = 3;
           obj[""] = 4;
           obj[0] = 5;
           obj[Symbol.for("moon")] = 6;
           return obj;
       })
g.eval("fill(this);\n" +
       "with (fill({})) h();");

for (var names of [withNames, globalNames]) {
    assertEq(names.indexOf("sanityCheck") !== -1, true);
    assertEq(names.indexOf("0xcafe"), -1);
    assertEq(names.indexOf(" "), -1);
    assertEq(names.indexOf(""), -1);
    assertEq(names.indexOf("0"), -1);
    assertEq(names.indexOf(Symbol.for("moon")), -1);
}
