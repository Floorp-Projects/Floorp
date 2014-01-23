// Test offThreadCompileScript option handling.

offThreadCompileScript('Error()');
assertEq(!!runOffThreadScript().stack.match(/^@<string>:1\n/), true);

offThreadCompileScript('Error()',
                       { fileName: "candelabra", lineNumber: 6502 });
assertEq(!!runOffThreadScript().stack.match(/^@candelabra:6502\n/), true);

var element = {};
offThreadCompileScript('Error()', { element: element }); // shouldn't crash
runOffThreadScript();

var elementAttribute = "molybdenum";
elementAttribute += elementAttribute + elementAttribute + elementAttribute;
offThreadCompileScript('Error()', { elementProperty: elementAttribute }); // shouldn't crash
runOffThreadScript();
