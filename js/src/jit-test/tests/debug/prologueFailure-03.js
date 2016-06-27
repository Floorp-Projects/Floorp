g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    let calledTimes = 0;
    Debugger(parent).onExceptionUnwind = function(frame) {
        switch (calledTimes++) {
          case 0:
            assertEq(frame.older.type, "global");
            break;
          case 1:
            // Force toplevel to return placidly so that we can tell assertions
            // from the throwing in the test.
            assertEq(frame.older, null);
            return { return: undefined };
          default:
            assertEq(false, true);
        }
    }
} + ")()");

var handler = {
    get() {
        r;
    }
};
new(new Proxy(function() {}, handler));
