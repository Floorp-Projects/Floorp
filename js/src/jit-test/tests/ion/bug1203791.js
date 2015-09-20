function n(x) {
    try {
	Object.create(x);
    } catch(e){};
}
function m() {
    n();
}
var g = newGlobal();
g.parent = this;
g.eval(`
    var dbg = new Debugger();
    var parentw = dbg.addDebuggee(parent);
    var pw = parentw.makeDebuggeeValue(parent.p);
    var scriptw = pw.script;
`);
g.dbg.onIonCompilation = function(graph) {
    if (graph.scripts[0] != g.scriptw)
        return;
    m();
};
function p() {
    for (var res = false; !res; res = inIon()) {}
}
p();
(function() {})();
