/*
 * Script.prototype.source should be an object. Moreover, it should be the
 * same object for each child script within the same debugger.
 */
let g = newGlobal();
let dbg = new Debugger(g);

let count = 0;
dbg.onNewScript = function (script) {
    assertEq(typeof script.source, "object");
    function traverse(script) {
        ++count;
        script.getChildScripts().forEach(function (child) {
            assertEq(child.source, script.source);
            traverse(child);
        });
    }
    traverse(script);
}

g.eval("2 * 3");
g.eval("function f() {}");
g.eval("function f() { function g() {} }");
g.eval("eval('2 * 3')");
g.eval("new Function('2 * 3')");
assertEq(count, 10);
