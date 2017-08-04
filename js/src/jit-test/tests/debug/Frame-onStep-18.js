// Regression test for bug 1370648.

let g = newGlobal();

let dbg = Debugger(g);
let lines = [0, 0, 0, 0, 0];
dbg.onDebuggerStatement = function (frame) {
  let dLine = frame.script.getOffsetLocation(frame.offset).lineNumber;
  lines[0] = 1;
  frame.onStep = function () {
    lines[frame.script.getOffsetLocation(this.offset).lineNumber - dLine] = 1;
  };
}

let s = `
      debugger;                 // 0
      if (1 !== 1) {            // 1
        print("dead code!?");   // 2
      }                         // 3
`;
g.eval(s);
assertEq(lines.join(""), "11001");
