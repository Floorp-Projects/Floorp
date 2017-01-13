// |jit-test| --ion-gvn=off;

try  {
    var g = newGlobal();
    var dbg = new Debugger(g);
    dbg.onExceptionUnwind = function(m) {
        do {
            m = m.older;
        } while (m != null);
    };
    g.eval("try { throw (function() {});} finally {}");
} catch(e) {

}
