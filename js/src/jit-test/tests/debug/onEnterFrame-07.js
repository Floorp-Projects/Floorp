var g = newGlobal();
var dbg = new Debugger(g);
var visibleFrames = 0;
dbg.onEnterFrame = function (frame) {
  print("> " + frame.script.url);
  visibleFrames++;
}

g.eval("(" + function iife() {
  [1].forEach(function noop() {});
  for (let x of [1]) {}
} + ")()");

// 1 for eval, 1 for iife(), 1 for noop()
assertEq(visibleFrames, 3);
