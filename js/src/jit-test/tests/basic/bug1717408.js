let g = newGlobal({newCompartment: true});
g.evaluate("obj = {}")
g.obj.__proto__ = {};
recomputeWrappers();
g.evaluate("obj.x = 1");
