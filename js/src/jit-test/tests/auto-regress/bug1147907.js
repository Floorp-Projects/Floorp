var evalInFrame = (function (global) {
  var dbgGlobal = newGlobal();
  var dbg = new dbgGlobal.Debugger();
  return function evalInFrame(upCount, code) {
    dbg.addDebuggee(global);
    var frame = dbg.getNewestFrame().older;
    var completion = frame.eval(code);
  };
})(this);
var x = 5;
{
  let x = eval("this.x++");
  evalInFrame(0, ("for (var x = 0; x < 3; ++x) { (function(){})() } "))
}
