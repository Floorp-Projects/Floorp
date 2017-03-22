
if (!wasmIsSupported())
    quit();

var sandbox = newGlobal();
var dbg = new Debugger(sandbox);
var counter = 0;
dbg.onExceptionUnwind = (frame, value) => {
    if (frame.type !== "wasmcall")
        return;
    if (++counter != 2)
        return;
    gc();
};

sandbox.innerCode = wasmTextToBinary(`(module
    (import "imports" "tbl" (table 1 anyfunc))
    (import $setNull "imports" "setNull" (func))
    (func $trap
        call $setNull
        unreachable
    )
    (elem (i32.const 0) $trap)
)`);
sandbox.outerCode = wasmTextToBinary(`(module
    (import "imports" "tbl" (table 1 anyfunc))
    (type $v2v (func))
    (func (export "run")
        i32.const 0
        call_indirect $v2v
    )
)`);

sandbox.eval(`
(function() {

var tbl = new WebAssembly.Table({initial:1, element:"anyfunc"});
function setNull() { tbl.set(0, null) }
new WebAssembly.Instance(new WebAssembly.Module(innerCode), {imports:{tbl,setNull}});
var outer = new WebAssembly.Instance(new WebAssembly.Module(outerCode), {imports:{tbl}});
var caught;
try {
    outer.exports.run();
} catch (e) {
    caught = e;
}
assertEq(caught instanceof WebAssembly.RuntimeError, true);

})();
`);
