
evalInFrame = function(global) {
   dbgGlobal = newGlobal()
   dbg = new dbgGlobal.Debugger
   return function(upCount, code) {
       dbg.addDebuggee(global)
       var frame = dbg.getNewestFrame().older
       for (var i = 0; i < upCount; i++)
           if (!frame) frame = older
       completion = frame.eval(code)
   }
}(this);
function h() {
    evalInFrame(0, "")
    evalInFrame(0, "i")
    evalInFrame(0, "a.push")
    evalInFrame(1, "a.pushy")
}
function g() h()
function f() g()
f()
evaluate(`
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
g()
h()
`);
