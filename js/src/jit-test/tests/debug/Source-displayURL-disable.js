// |jit-test| --no-source-pragmas

const g = newGlobal({ newCompartment: true });
const dbg = Debugger(g);
let source;
dbg.onDebuggerStatement = function (frame) {
  source = frame.script.source;
};

g.eval(`
  debugger;
  //# sourceURL=file.js
`);
assertEq(source.displayURL, null);
