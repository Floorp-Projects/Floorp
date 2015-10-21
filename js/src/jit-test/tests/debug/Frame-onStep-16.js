// Stepping through a function with a return statement should pause on
// the closing brace of the function.

var g = newGlobal();
var dbg = Debugger(g);
var log;

function test(fnStr) {
  log = '';
  g.eval(fnStr);

  dbg.onDebuggerStatement = function(frame) {
        frame.onStep = function() {
      let {lineNumber, isEntryPoint} = frame.script.getOffsetLocation(frame.offset);
      if (isEntryPoint) {
        log += lineNumber + ' ';
      }
    };
  };

  g.eval("f(23);");
}

test("function f(x) {\n" +    // 1
     "    debugger;\n" +      // 2
     "    return 23 + x;\n" + // 3
     "}\n");                  // 4
assertEq(log, '3 4 ');

test("function f(x) {\n" +    // 1
     "    debugger;\n" +      // 2
     "    return;\n" +        // 3
     "}\n");                  // 4
assertEq(log, '3 4 ');
