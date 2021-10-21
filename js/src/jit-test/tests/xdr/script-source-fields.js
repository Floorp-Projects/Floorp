const code = `
debugger;
`;
let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);
const bytes = g.compileToStencilXDR(code, {
  fileName: "test-filename.js",
  lineNumber: 12,
  forceFullParse: true,
  displayURL: "display-url.js",
  sourceMapURL: "source-map-url.js",
});
let hit = false;
dbg.onDebuggerStatement = frame => {
  hit = true;
  const source = frame.script.source;
  assertEq(source.url, "test-filename.js");
  assertEq(source.displayURL, "display-url.js");
  assertEq(source.sourceMapURL, "source-map-url.js");
  assertEq(source.startLine, 12);
};
g.evalStencilXDR(bytes, {});
assertEq(hit, true);
