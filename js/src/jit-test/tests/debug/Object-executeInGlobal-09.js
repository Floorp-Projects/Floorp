// The frame created for executeInGlobal is never marked as a 'FUNCTION' frame.

(function () {
  var g = newGlobal({newCompartment: true});
  var dbg = new Debugger;
  var gw = dbg.addDebuggee(g);
  gw.executeInGlobalWithBindings("eval('Math')",{}).return
})();

