// Test how stepping interacts with switch statements.

var g = newGlobal();

g.eval('function bob() { return "bob"; }');

// Stepping into a sparse switch should not stop on literal cases.
evaluate(`function lit(x) {     // 1
  debugger;                     // 2
  switch(x) {                   // 3
  case "nope":                  // 4
    break;                      // 5
  case "bob":                   // 6
    break;                      // 7
  }                             // 8
}`, {lineNumber: 1, global: g});

// Stepping into a sparse switch should stop on non-literal cases.
evaluate(`function nonlit(x) {  // 1
  debugger;                     // 2
  switch(x) {                   // 3
  case bob():                   // 4
    break;                      // 5
  }                             // 6
}`, {lineNumber: 1, global: g});

var dbg = Debugger(g);
var badStep = false;

function test(s, okLine) {
  dbg.onDebuggerStatement = function(frame) {
    frame.onStep = function() {
      let thisLine = this.script.getOffsetLocation(this.offset).lineNumber;
      // The stop at line 3 is the switch.
      if (thisLine > 3) {
        assertEq(thisLine, okLine)
        frame.onStep = undefined;
      }
    };
  };
  g.eval(s);
}

test("lit('bob');", 7);

test("nonlit('bob');", 4);
