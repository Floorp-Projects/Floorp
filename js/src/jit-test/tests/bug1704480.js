// |jit-test|  --more-compartments
m = parseModule(`
    await 0 ? b : c
`);
moduleLink(m);
moduleEvaluate(m)
d = newGlobal();
d.e = this;
d.eval(`
 Debugger(e).onExceptionUnwind = function(f) {
    print(f.eval)
    return f.eval("")
 }
`);
