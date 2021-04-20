// |jit-test| skip-if: helperThreadCount() === 0

// Test offThreadCompileScript option handling.

offThreadCompileScript("Error()");
assertEq(!!runOffThreadScript().stack.match(/^@<string>:1:1\n/), true);

offThreadCompileScript("Error()", { fileName: "candelabra", lineNumber: 6502 });
assertEq(!!runOffThreadScript().stack.match(/^@candelabra:6502:1\n/), true);

var element = {};
var job = offThreadCompileScript("Error()"); // shouldn't crash
runOffThreadScript(job, { element });

var elementAttributeName = "molybdenum";
elementAttributeName +=
  elementAttributeName + elementAttributeName + elementAttributeName;
job = offThreadCompileScript("Error()"); // shouldn't crash
runOffThreadScript(job, {
  elementAttributeName,
});
