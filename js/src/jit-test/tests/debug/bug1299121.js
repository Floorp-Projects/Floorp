var g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    var dbg = new Debugger(parent);
    dbg.onExceptionUnwind = function(frame) {
        frame.eval("h = 3");
    };
} + ")()");
g = function h() { }
g();
