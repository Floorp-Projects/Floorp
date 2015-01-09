// |jit-test| test-also-noasmjs
load(libdir + "asm.js");

if (!isAsmJSCompilationAvailable())
    quit();

var module = "'use asm';\n";
for (var i = 0; i < 100; i++) {
    module += "function f" + i + "(i) {\n";
    module += "  i=i|0; var j=0; j=(i+1)|0; i=(j-4)|0; i=(i+j)|0; return i|0\n";
    module += "}\n";
}
module += "return f0";
var script = "(function() {\n" + module + "})";

for (var i = 0; i < 10; i++) {
    try {
        offThreadCompileScript(script);
        var f = new Function(module);
        var g = runOffThreadScript();
        assertEq(isAsmJSModule(f), true);
        assertEq(isAsmJSModule(g), true);
    } catch (e) {
        // ignore spurious error when offThreadCompileScript can't run in
        // parallel
    }
}
