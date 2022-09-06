// |jit-test| skip-if: !isAsmJSCompilationAvailable() || helperThreadCount() === 0

load(libdir + "asm.js");

var module = "'use asm';\n";
for (var i = 0; i < 100; i++) {
    module += "function f" + i + "(i) {\n";
    module += "  i=i|0; var j=0; j=(i+1)|0; i=(j-4)|0; i=(i+j)|0; return i|0\n";
    module += "}\n";
}
module += "return f0";
var script = "(function() {\n" + module + "})";

for (var i = 0; i < 10; i++) {
    offThreadCompileToStencil(script);
    var f = new Function(module);
    var stencil = finishOffThreadStencil();
    var g = evalStencil(stencil);
    assertEq(isAsmJSModule(f), true);
    assertEq(isAsmJSModule(g), true);
}
