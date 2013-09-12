// Bug 857050: Remove the timeout function root before shutting down.
function timeoutfunc() {}
timeout(1, timeoutfunc);
var g = newGlobal();
var dbg = Debugger(g);
