let g = newGlobal({newCompartment: true});
new Debugger(g).onExceptionUnwind = () => null;
g.eval(`import("javascript: throw 1")`).catch(() => 0);
