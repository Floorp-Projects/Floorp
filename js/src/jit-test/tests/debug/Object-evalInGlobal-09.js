// The frame created for evalInGlobal is never marked as a 'FUNCTION' frame.

(function () {
  var g = newGlobal();
  var dbg = new Debugger;
  var gw = dbg.addDebuggee(g);
  gw.evalInGlobalWithBindings("eval('Math')",{}).return
})();

