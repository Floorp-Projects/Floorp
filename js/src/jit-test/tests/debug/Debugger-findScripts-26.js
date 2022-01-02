// |jit-test| skip-if: !('oomTest' in this)
var g = newGlobal({newCompartment: true});
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval(`
function f() {
    function inner() {
        var x;
    }
}
`);

let url = thisFilename();
let line = 4;

oomTest(() => { dbg.findScripts({url, line}) });
oomTest(() => { dbg.findScripts({url, line, innermost: true}) });
