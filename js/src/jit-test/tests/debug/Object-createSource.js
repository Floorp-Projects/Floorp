// createSource creates new sources.

let g = newGlobal({ newCompartment: true });
let dbg = new Debugger(g);
let gdbg = dbg.addDebuggee(g);

let source = gdbg.createSource({
  text: "x = 3",
  url: "foo.js",
  startLine: 3,
  startColumn: 42,
  sourceMapURL: "sourceMapURL",
  isScriptElement: true,
});
assertEq(source.text, "x = 3");
assertEq(source.url, "foo.js");
assertEq(source.startLine, 3);
assertEq(source.startColumn, 42);
assertEq(source.sourceMapURL, "sourceMapURL");
assertEq(source.introductionType, "inlineScript");
