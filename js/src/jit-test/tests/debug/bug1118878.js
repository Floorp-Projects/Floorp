
var evalInFrame = (function (global) {
  var dbgGlobal = newGlobal();
  var dbg = new dbgGlobal.Debugger();
  return function evalInFrame(upCount, code) {
    dbg.addDebuggee(global);
    var frame = dbg.getNewestFrame().older;
    var completion = frame.eval(code);
  };
})(this);
evaluate("for (var k in 'xxx') (function g() { Math.atan2(42); evalInFrame((0), (''), true); })();", { noScriptRval : true, isRunOnce : true });
