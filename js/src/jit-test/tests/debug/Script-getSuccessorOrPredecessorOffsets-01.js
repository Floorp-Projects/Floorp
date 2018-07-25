/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */

// An offset A of a script should have successor B iff B has predecessor A.
// Offsets reached while stepping through a script should be successors of
// each other.

var scripts = [
  "n = 1",
  "if (n == 0) return; else n = 2",
  "while (n < 5) n++",
  "switch (n) { case 4: break; case 5: return 0; }",
];

var g = newGlobal();
for (var n in scripts) {
  g.eval("function f" + n + "() { " + scripts[n] + " }");
}

var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
  for (var n in scripts) {
    var compares = 0;
    var script = frame.eval("f" + n).return.script;
    var worklist = [script.mainOffset];
    var seen = [];
    while (worklist.length) {
      var offset = worklist.pop();
      if (seen.includes(offset))
        continue;
      seen.push(offset);
      var succs = script.getSuccessorOffsets(offset);
      for (var succ of succs) {
        compares++;
        var preds = script.getPredecessorOffsets(succ);
        assertEq(preds.includes(offset), true);
        worklist.push(succ);
      }
    }
    assertEq(compares != 0, true);
    compares = 0;
    for (var offset of seen) {
      var preds = script.getPredecessorOffsets(offset);
      for (var pred of preds) {
        var succs = script.getSuccessorOffsets(pred);
        compares++;
        assertEq(succs.includes(offset), true);
      }
    }
    assertEq(compares != 0, true);
    script.setBreakpoint(script.mainOffset, { hit: mainBreakpointHandler });
  }
};
g.eval("debugger");

function mainBreakpointHandler(frame) {
  frame.lastOffset = frame.script.mainOffset;
  frame.onStep = onStepHandler;
}

function onStepHandler() {
  steps++;
  var succs = this.script.getSuccessorOffsets(this.lastOffset);
  if (!succs.includes(this.offset)) {
    // The onStep handler might skip over opcodes, even when running in the
    // interpreter. Check transitive successors of the last offset.
    var found = false;
    for (var succ of succs) {
      if (this.script.getSuccessorOffsets(succ).includes(this.offset))
        found = true;
    }
    assertEq(found, true);
  }
  this.lastOffset = this.offset;
}

for (var n in scripts) {
  var steps = 0;
  g.eval("f" + n + "()");
  assertEq(steps != 0, true);
}
