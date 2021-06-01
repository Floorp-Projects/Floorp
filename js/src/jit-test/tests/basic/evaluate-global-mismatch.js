load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});

let g_discardSource = newGlobal({
  discardSource: true,
});

assertErrorMessage(() => {
  // Script is compiled with discardSource=false, and is executed in global
  // with discardSource=true.
  g.evaluate("function f() { (function(){})(); }; f();", {
    global: g_discardSource,
  });
}, g_discardSource.Error, "discardSource option mismatch");
