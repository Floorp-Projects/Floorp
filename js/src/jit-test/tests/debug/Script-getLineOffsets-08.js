// A "while" or a "for" loop should have a single entry point.

var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = function(frame) {
  var s = frame.eval('f').return.script;

  // There should be just a single entry point for the first line of
  // the function.  See below to understand the "+2".
  assertEq(s.getLineOffsets(g.line0 + 2).length, 1);
};


function test(code) {
  g.eval('var line0 = Error().lineNumber;\n' +
         'function f() {\n' +   // line0 + 1
         code + '\n' +          // line0 + 2 -- see above
         '}\n' +
         'debugger;');
}

test('while (false)\n;');
test('for (;false;)\n;');
test('for (;;) break;\n;');
