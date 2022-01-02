// Test how stepping interacts with for-in/of statements.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var log;
var previous;

dbg.onDebuggerStatement = function (frame) {
  let debugLine = frame.script.getOffsetLocation(frame.offset).lineNumber;
  log = '';
  previous = '';
  frame.onStep = function() {
    let foundLine = this.script.getOffsetLocation(this.offset).lineNumber;
    if (this.script.getLineOffsets(foundLine).indexOf(this.offset) >= 0) {
      let thisline = (foundLine - debugLine).toString(16);
      if (thisline !== previous) {
        log += thisline;
        previous = thisline;
      }
    }
  };
};

function testOne(decl, loopKind) {
  let body = "var array = [2, 4, 6];\ndebugger;\nfor (" + decl + " iter " +
      loopKind + " array) {\n  print(iter);\n}\n";
  g.eval(body);
  assertEq(log, "12121214");
}

for (let decl of ["", "var", "let"]) {
  testOne(decl, "in");
  testOne(decl, "of");
}
