Object.defineProperty(this, "x", {});
try {
  oomTest(function() {
      let m = parseModule(``);
  });
  var dbg = new Debugger;
  dbg.onNewGlobalObject = function (global) {}
  let t39 = {};
  addMarkObservers([t39]);
  let g33 = newGlobal({newCompartment: true});
  g33.t39 = t39;
  g33.eval("grayRoot().push(t);");
} catch (exc) {}
gcparam("allocationThreshold", 1 /* MB */);
gcparam("incrementalGCEnabled", false);
oomTest(function() {
  var lfGlobal = newGlobal({sameZoneAs: this});
});
