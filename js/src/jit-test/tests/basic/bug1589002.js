var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval(`
  x = "12";
  x += "3";
  parent.evaluate("", {global: this, sourceMapURL: x});
`);
