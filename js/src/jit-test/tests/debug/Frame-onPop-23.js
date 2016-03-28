// Check that the line number reported at an onPop stop makes sense,
// even when it happens on an "artificial" instruction.

var g = newGlobal();

// This bit of code arranges for the line number of the "artificial"
// instruction to be something nonsensical -- the middle of a loop
// which cannot be entered.
g.eval(`function f() {
  debugger;                   // +0
  if(false) {                 // +1
    for(var b=0; b<0; b++) {  // +2
      c = 2;                  // +3
    }                         // +4
  }                           // +5
}                             // +6
`);

var dbg = Debugger(g);

let debugLine;
let foundLine;

dbg.onDebuggerStatement = function(frame) {
  debugLine = frame.script.getOffsetLocation(frame.offset).lineNumber;
  frame.onPop = function(c) {
    foundLine = this.script.getOffsetLocation(this.offset).lineNumber;
  };
};

g.eval("f();\n");

// The stop should happen on the closing brace of the function.
assertEq(foundLine == debugLine + 6, true);
