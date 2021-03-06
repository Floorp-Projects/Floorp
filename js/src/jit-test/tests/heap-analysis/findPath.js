load(libdir + "match.js")

// At the moment, findPath just returns the names as provided by ubi::Node,
// which just uses js::TraceChildren for now. However, we have various plans
// to improve the quality of ubi::Node's metadata, to improve the precision
// and clarity of the results here.

var o = { w: { x: { y: { z: {} } } } };
Match.Pattern([{node: {}, edge: "w"},
               {node: {}, edge: "x"},
               {node: {}, edge: "y"},
               {node: {}, edge: "z"}])
  .assert(findPath(o, o.w.x.y.z));
print(JSON.stringify(findPath(o, o.w.x.y.z)));

var a = [ , o ];
Match.Pattern([{node: {}, edge: "objectElements[1]"}])
  .assert(findPath(a, o));
print(JSON.stringify(findPath(a, o)));

function C() {}
C.prototype.obj = {};
var c = new C;
Match.Pattern([{node: {}, edge: "group"},
               {node: Match.Pattern.ANY, edge: "group_proto"},
               {node: { constructor: Match.Pattern.ANY }, edge: "obj"}])
  .assert(findPath(c, c.obj));
print(JSON.stringify(findPath(c, c.obj)));

function f(x) { return function g(y) { return x+y; }; }
var o = {}
var gc = f(o);
Match.Pattern([{node: gc, edge: "fun_environment"},
               {node: Match.Pattern.ANY, edge: "x"}])
  .assert(findPath(gc, o));
print(JSON.stringify(findPath(gc, o)));

Match.Pattern([{node: {}, edge: "shape"},
               {node: Match.Pattern.ANY, edge: "base"},
               {node: Match.Pattern.ANY, edge: "baseshape_global"},
               {node: {}, edge: "o"}])
  .assert(findPath(o, o));
print(findPath(o, o).map((e) => e.edge).toString());

// Check that we can generate ubi::Nodes for Symbols.
var so = { sym: Symbol() };
Match.Pattern([{node: {}, edge: "sym" }])
  .assert(findPath(so, so.sym));
print(findPath(so, so.sym).map((e) => e.edge).toString());
