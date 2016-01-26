// Stepping over a not-taken "if" that is at the end of the function
// should move to the end of the function, not somewhere in the body
// of the "if".

var g = newGlobal();
g.eval(`function f() {        // 1
  var a,c;                    // 2
  debugger;                   // 3
  if(false) {                 // 4
    for(var b=0; b<0; b++) {  // 5
      c = 2;                  // 6
    }                         // 7
  }                           // 8
}                             // 9
`);

var dbg = Debugger(g);
var badStep = false;

dbg.onDebuggerStatement = function(frame) {
  let debugLine = frame.script.getOffsetLocation(frame.offset).lineNumber;
  assertEq(debugLine, 3);
  frame.onStep = function() {
    let foundLine = this.script.getOffsetLocation(this.offset).lineNumber;
    assertEq(foundLine <= 4 || foundLine >= 8, true);
  };
};

g.eval("f();\n");
