// Test Script.lineCount.

var g = newGlobal();
var dbg = Debugger(g);

function test(scriptText, expectedLineCount) {
  let found = false;

  dbg.onNewScript = function(script, global) {
    assertEq(script.lineCount, expectedLineCount);
    found = true;
  };

  g.evaluate(scriptText);
  assertEq(found, true);
}

src = 'var a = (function(){\n' + // 0
      'var b = 9;\n' +           // 1
      'console.log("x", b);\n'+  // 2
      'return b;\n' +            // 3
      '})();\n';                 // 4
test(src, 5);
