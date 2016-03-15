g = newGlobal();
g.parent = this;

function installHook() {
    let calledTimes = 0;
    function hook(frame) {
        calledTimes++;
        switch (calledTimes) {
          case 1:
            // Proxy get trap
            assertEq(frame.type, "call");
            assertEq(frame.script.displayName.includes("get"), true);
            break;
          case 2:
            // wrapper function. There is no entry for notRun
            assertEq(frame.type, "call");
            assertEq(frame.script.displayName.includes("wrapper"), true);
            break;
          case 3:
            assertEq(frame.type, "global");
            // Force the top-level to return cleanly, so that we can tell
            // assertion failures from the intended throwing.
            return { return: undefined };

          default:
            // that's the whole chain.
            assertEq(false, true);
        }
    }

    Debugger(parent).onExceptionUnwind = hook;
}


g.eval("(" + installHook + ")()");

var handler = {
    get(t, p) {
        throw new TypeError;
    }
};

function notRun() {}

function wrapper() {
    var f = new Proxy(notRun, handler);
    new f();
}
wrapper();
