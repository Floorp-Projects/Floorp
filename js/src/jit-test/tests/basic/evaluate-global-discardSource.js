load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});

let g_discardSource = newGlobal({
  discardSource: true,
});

g.evaluate("function f() { (function(){})(); }; f();", {
  global: g_discardSource,
});
