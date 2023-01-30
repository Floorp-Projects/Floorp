// |jit-test| --fast-warmup; --no-threads
function foo(n) {
  with ({}) {}
  if (n == 9) {
    enableTrackAllocations();
    var g = newGlobal({newCompartment: true});
    var dbg = g.Debugger(this);
    dbg.getNewestFrame().older;
  }
}
for (var i = 0; i < 10; i++) {
  for (var j = 0; j < 20; j++) {}
  foo(i);
}
