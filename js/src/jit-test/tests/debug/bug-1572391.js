g = newGlobal({ newCompartment: true });
dbg = Debugger(g);
dbg.onDebuggerStatement = function(frame) {
  frame.older
}

g.eval(`
  function* countdown(recur) {
    if (recur)
      yield* countdown(false);
    debugger;
  }
`);

g.countdown(true).next()
