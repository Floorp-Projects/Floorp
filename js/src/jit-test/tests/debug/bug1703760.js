let g = newGlobal({newCompartment: true});

Debugger(g).onEnterFrame = function(fr) {
    fr.eval("")
};

gczeal(11);
gczeal(9, 9);

g.eval(`
  var count = 0;
  function inner() {
    if (++count < 20) {
      inner();
    }
  }
  inner();
`);
