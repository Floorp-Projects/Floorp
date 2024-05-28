var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval(`
  Debugger(parent).onEnterFrame = function() {
    newGlobal({newCompartment: true}).eval('import("")').catch(Object);
  };
`);
stackTest(function(){}, {keepFailing: true});
