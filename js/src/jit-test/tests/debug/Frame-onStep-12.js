// Because our script source notes record only those bytecode offsets
// at which source positions change, the default behavior in the
// absence of a source note is to attribute a bytecode instruction to
// the same source location as the preceding instruction. When control
// flows from the preceding bytecode to the one we're emitting, that's
// usually plausible. But successors in the bytecode stream are not
// necessarily successors in the control flow graph. If the preceding
// bytecode was a back edge of a loop, or the jump at the end of a
// 'then' clause, its source position can be completely unrelated to
// that of its successor.

// We try to avoid showing such nonsense offsets to the user by
// requiring breakpoints and single-stepping to stop only at a line's
// entry points, as reported by Debugger.Script.prototype.getLineOffsets;
// and then ensuring that those entry points are all offsets mentioned
// explicitly in the source notes, and hence deliberately attributed
// to the given bytecode.

// This bit of JavaScript compiles to bytecode ending in a branch
// instruction whose source position is the body of an unreachable
// loop. The first instruction of the bytecode we emit following it
// will inherit this nonsense position, if we have not explicitly
// emitted a source note for said instruction.

// This test steps across such code and verifies that control never
// appears to enter the unreachable loop.

var bitOfCode = `debugger;                    // +0
                 if(false) {                  // +1
                   for(var b=0; b<0; b++) {   // +2
                      c = 2                   // +3
                    }                         // +4
                 }`;                          // +5

var g = newGlobal();
var dbg = Debugger(g);

g.eval("function nothing() { }\n");

var log = '';
dbg.onDebuggerStatement = function(frame) {
  let debugLine = frame.script.getOffsetLocation(frame.offset).lineNumber;
  frame.onStep = function() {
    let foundLine = this.script.getOffsetLocation(this.offset).lineNumber;
    if (this.script.getLineOffsets(foundLine).indexOf(this.offset) >= 0) {
      log += (foundLine - debugLine).toString(16);
    }
  };
};

function testOne(name, body, expected) {
  print(name);
  log = '';
  g.eval(`function ${name} () { ${body} }`);
  g.eval(`${name}();`);
  assertEq(log, expected);
}



// Test the instructions at the end of a "try".
testOne("testTryFinally",
        `try {
           ${bitOfCode}
         } finally {            // +6
         }                      // +7
         nothing();             // +8
        `, "1689");

// The same but without a finally clause.
testOne("testTryCatch",
        `try {
           ${bitOfCode}
         } catch (e) {          // +6
         }                      // +7
         nothing();             // +8
        `, "189");

// Test the instructions at the end of a "catch".
testOne("testCatchFinally",
        `try {
           throw new TypeError();
         } catch (e) {
           ${bitOfCode}
         } finally {            // +6
         }                      // +7
         nothing();             // +8
        `, "1689");

// Test the instruction at the end of a "finally" clause.
testOne("testFinally",
        `try {
         } finally {
           ${bitOfCode}
         }                      // +6
         nothing();             // +7
        `, "178");

// Test the instruction at the end of a "then" clause.
testOne("testThen",
        `if (1 === 1) {
           ${bitOfCode}
         } else {               // +6
         }                      // +7
         nothing();             // +8
        `, "189");

// Test the instructions leaving a switch block.
testOne("testSwitch",
        `var x = 5;
         switch (x) {
           case 5:
             ${bitOfCode}
         }                      // +6
         nothing();             // +7
        `, "178");
