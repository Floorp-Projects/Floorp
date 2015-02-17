// |jit-test| error: InternalError
x = {}; 
y = x;
if (x !== y) {}
Object.defineProperty(this, "x", {
    get: function() {
    Object.defineProperty(this, "y", {
        get: function() {
        return Proxy(this.y)
        }
    });
    }
})
x;
g = newGlobal();
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})")
y; 
