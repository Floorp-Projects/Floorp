// Test offThreadCompileScript option handling.

if (helperThreadCount() === 0)
  quit(0);

offThreadCompileScript('Error()');
assertEq(!!runOffThreadScript().stack.match(/^@<string>:1:1\n/), true);

offThreadCompileScript('Error()',
                       { fileName: "candelabra", lineNumber: 6502 });
assertEq(!!runOffThreadScript().stack.match(/^@candelabra:6502:1\n/), true);

var element = {};
offThreadCompileScript('Error()', { element: element }); // shouldn't crash
runOffThreadScript();

var elementAttributeName = "molybdenum";
elementAttributeName += elementAttributeName + elementAttributeName + elementAttributeName;
offThreadCompileScript('Error()', { elementAttributeName: elementAttributeName }); // shouldn't crash
runOffThreadScript();
