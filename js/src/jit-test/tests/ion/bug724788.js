// |jit-test| error: ReferenceError
o0 = TypeError.prototype.__proto__
o1 = new Proxy({}, {})
o13 = {}.__proto__
var o15 = Object.prototype
o31 = (new Uint32Array(100)).buffer
function f2(o) {
  try {
    ({
      x: [eval("o")][0]
    }.x.__defineGetter__("toString", function() {
      return o26;
    }));
  } catch (e) {}
}
function f3(o) {
  try {
    +o31
  } catch (e) {}
}
function f19(o) {
  for (var x in eval("o")) {
    eval("o")[x];
  }
}
f2(o15)
f3(o0)
f19(o13)

