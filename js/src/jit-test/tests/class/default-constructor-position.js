// Test default class constructors have reasonable lineno/column values

const source = `
  /* GeneralParser::synthesizeConstructor */ class A {
  }

  /* GeneralParser::synthesizeConstructor (derived) */ class B extends A {
  }

  /* GeneralParser::synthesizeConstructor */ class C {
    field = "default value";
  }

  /* GeneralParser::synthesizeConstructor (derived) */ class D extends A {
    field = "default value";
  }
`;

// Use the Debugger API to introspect the line / column.
let d = new Debugger();
let g = newGlobal({newCompartment: true})
let gw = d.addDebuggee(g);

g.evaluate(source);

function getStartLine(name) {
  return gw.makeDebuggeeValue(g.eval(name)).script.startLine;
}

function getStartColumn(name) {
  return gw.makeDebuggeeValue(g.eval(name)).script.startColumn;
}

function getSourceStart(name) {
  return gw.makeDebuggeeValue(g.eval(name)).script.sourceStart;
}

function getSourceLength(name) {
  return gw.makeDebuggeeValue(g.eval(name)).script.sourceLength;
}

// Compute the expected line/column from source.
matches = "";
lineno = 0;
for (text of source.split("\n")) {
  lineno++;

  column = text.indexOf("class");
  if (column < 0) {
    continue;
  }

  className = text[column + 6];
  matches += className;

  // Check lineno/column.
  assertEq(getStartLine(className), lineno);
  assertEq(getStartColumn(className), column + 1);

  // Check sourceStart/sourceEnd.
  offset = source.indexOf("class " + className)
  length = source.substring(offset).indexOf("}") + 1
  assertEq(getSourceStart(className), offset)
  assertEq(getSourceLength(className), length)
}

// Sanity check to did actual matches
assertEq(matches, "ABCD");
