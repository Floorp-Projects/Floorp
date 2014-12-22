var evalInFrame = (function (global) {
  var dbgGlobal = newGlobal();
  var dbg = new dbgGlobal.Debugger();
  return function evalInFrame(upCount, code) {
    dbg.addDebuggee(global);
    var frame = dbg.getNewestFrame().older;
    var completion = frame.eval(code);
  };
})(this);
function g1(x, args) {}
function f1(x, y, o) {
    for (var i=0; i<50; i++) {
        o.apply(evalInFrame(0, "x"), x);
    }
}
var o1 = {apply: g1};
assertEq(f1(3, 5, o1), undefined);
