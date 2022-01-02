var g27 = newGlobal({newCompartment: true});
g27.debuggeeGlobal = this;
g27.eval(`
  dbg = new Debugger(debuggeeGlobal);
  dbg.onExceptionUnwind = function (frame, exc) {};
`);
s45 = newGlobal({newCompartment: true});
try {
  evalcx(`
      function h(h) {}
      h.valueOf=g;
  `, s45);
} catch (x) {}
try {
  evalcx("throw h", s45)
} catch (x) {}
gcslice(100000);
