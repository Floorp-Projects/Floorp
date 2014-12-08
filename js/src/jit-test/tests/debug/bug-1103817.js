// Random chosen test: js/src/jit-test/tests/debug/Source-introductionScript-04.js
x = (new Debugger).addDebuggee(newGlobal());
print(x.getOwnPropertyDescriptor('Function').value.proto.script);
// Random chosen test: js/src/jit-test/tests/debug/Memory-takeCensus-03.js
(new Debugger).memory.takeCensus();
