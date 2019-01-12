print = function(s) { return s.toString(); }
assertEq = function(a,b) {
  try { print(a); print(b); } catch(exc) {}
}
g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("(" + function() {
  Debugger(parent).onExceptionUnwind = function(frame) {
    frame.older
  }
} + ")()")
function a() {};
function b() {};
for (let _ of Array(100))
  assertEq(b instanceof a, true);
function c(){};
function d(){};
function e(){};
Object.defineProperty(a, Symbol.hasInstance, {value: assertEq });
let funcs = [a, b, c, d];
for (let f of funcs)
  assertEq(e instanceof f, true);

