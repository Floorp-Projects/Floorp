newGlobal({newCompartment: true});
g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
gczeal(2, 100);
function f(x, initFunc) {
    newGlobal({newCompartment: true});
    g.eval(`
        var binary = wasmTextToBinary('${x}');
        var offsets = wasmCodeOffsets(binary);
        new WebAssembly.Instance(new WebAssembly.Module(binary));
    `);
    var {
        offsets
    } = g;
    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
    initFunc({
        wasmScript,
        breakpoints: offsets
    })
};
try {
    f('(module (func nop nop) (export "" (func 0)))',
        function({
            wasmScript,
            breakpoints
        }) {
            breakpoints.forEach(function(offset) {
                wasmScript.setBreakpoint(offset, s = {});
            });
        }
    );
    f();
} catch (e) {}
