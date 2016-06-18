if (!('oomTest' in this))
    quit();

var o = { w: { x: { y: { z: {} } } } };
oomTest(() => findPath(o, o.w.x.y.z));

var a = [ , o ];
oomTest(() => findPath(a, o));

function C() {}
C.prototype.obj = {};
var c = new C;

oomTest(() => findPath(c, c.obj));

function f(x) { return function g(y) { return x+y; }; }
var o = {}
var gc = f(o);
oomTest(() => findPath(gc, o));
