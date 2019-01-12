// |jit-test| error: ReferenceError

evaluate(`
    var g = newGlobal({newCompartment: true});
    g.parent = this;
    g.eval('new Debugger(parent).onExceptionUnwind = function() {};');
`)
{
    while (x && 0) {}
    let x
}
