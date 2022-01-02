load(libdir + "wasm-binary.js");

const wasmEval = (code, imports) => new WebAssembly.Instance(new WebAssembly.Module(code), imports).exports;
const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);

// Deep nesting shouldn't crash or even throw. This test takes a long time to
// run with the JITs disabled, so to avoid occasional timeout, disable. Also
// in eager mode, this triggers pathological recompilation, so only run for
// "normal" JIT modes. This test is totally independent of the JITs so this
// shouldn't matter.
var jco = getJitCompilerOptions();
if (jco["ion.enable"] && jco["baseline.enable"] && jco["baseline.warmup.trigger"] > 0 && jco["ion.warmup.trigger"] > 10) {
    var manyBlocks = [];
    for (var i = 0; i < 20000; i++)
        manyBlocks.push(BlockCode, VoidCode);
    for (var i = 0; i < 20000; i++)
        manyBlocks.push(EndCode);
    wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:manyBlocks})])]));
}
