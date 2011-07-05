// Don't assert with dead Debugger.Object and live cross-compartment wrapper of referent.
var g = newGlobal('new-compartment');
for (var j = 0; j < 4; j++) {
    var dbg = new Debugger;
    dbg.addDebuggee(g);
    dbg.enabled = false;
    dbg = null;
    gc(); gc();
} 
