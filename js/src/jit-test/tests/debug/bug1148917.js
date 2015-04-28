// |jit-test| error: Error

var g = newGlobal();
g.eval('function f(a) { evaluate("f(" + " - 1);", {newContext: true}); }');
var dbg = new Debugger(g);
var frames = [];
dbg.onEnterFrame = function (frame) {
  if (frames.length == 3)
    return;
  frames.push(frame);
  for (var f of frames)
    f.eval('a').return
};
g.f();
