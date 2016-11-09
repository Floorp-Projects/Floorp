// |jit-test| error: ReferenceError

g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {}");
a = new class extends Array {
    constructor() {
        for (;; ([] = p)) {}
    }
}
