// |jit-test|  --enable-top-level-await; --more-compartments
m = parseModule(`
    await 0 ? b : c
`);
m.declarationInstantiation();
m.evaluation()
d = newGlobal();
d.e = this;
d.eval(`
 Debugger(e).onExceptionUnwind = function(f) {
    print(f.eval)
    return f.eval("")
 }
`);
