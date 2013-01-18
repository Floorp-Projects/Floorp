// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-32-11714be33655-linux
// Flags: -m -n
//
function f(a) {}
s = [{
  s: [],
  s: function(d, b) {},
  t: function() {
    try {} catch (e) {}
  }
}, {
  t: "",
  s: [],
  s: function(d, b) {}
}, {
  t: "",
  s: [],
  s: function(d, b) {},
  t: function() {}
}, {
  t: "",
  x: "",
  s: [],
  g: function(b) {},
  t: function(f) {}
}, {
  t: "",
  s: [],
  s: function() {}
}];
v = 0
Function("gc(evalcx('lazy'))")();
gczeal();
gc();
(function() {
  x
})()
