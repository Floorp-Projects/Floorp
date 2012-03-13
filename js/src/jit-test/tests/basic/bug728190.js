// |jit-test| slow;

function TestCase(n, d, e, a) {}
var lfcode = new Array();
lfcode.push("");
lfcode.push("\
  var summary = 'foo';\
  test();\
  function test() {\
    test(\"TEST-UNEXPECTED-FAIL | TestPerf | \" + summary);\
  }\
");
lfcode.push("gczeal(2);");
lfcode.push("\
  new TestCase(TestFunction_3( \"P\", \"A\", \"S\", \"S\" ) +\"\");\
  new TestCase(TestFunction_4( \"F\", \"A\", (\"length\"), \"L\" ) +\"\");\
  function TestFunction_3( a, b, c, d, e ) {\
    TestFunction_3(arguments);\
  }\
");
while (true) {
        var file = lfcode.shift(); if (file == undefined) { break; }
        try { evaluate(file); } catch (lfVare) {}
}
