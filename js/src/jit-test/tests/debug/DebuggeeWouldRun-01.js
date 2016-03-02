// Bug 1250190: Shouldn't crash. |jit-test| exitstatus: 3

g = newGlobal();
var dbg = Debugger(g)
dbg.onNewPromise = () => g.makeFakePromise();
g.makeFakePromise();

