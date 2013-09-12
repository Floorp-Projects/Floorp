// env.find() finds noneumerable properties in with statements.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
g.h = function () {
    var frame = dbg.getNewestFrame();
    var target = frame.eval("obj").return;
    var env = frame.environment.find("PI");
    assertEq(env.object, target);
    hits++;
};

g.obj = g.Math;
g.eval("with (obj) h();");
g.eval("with (Math) { let x = 12; h(); }");
g.eval("obj = {};\n" +
       "Object.defineProperty(obj, 'PI', {enumerable: false, value: 'Marlowe'});\n" +
       "with (obj) h();\n");
assertEq(hits, 3);
