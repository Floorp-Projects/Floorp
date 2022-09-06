// |jit-test| slow; skip-if: helperThreadCount() === 0

var s = '';
for (var i = 0; i < 70000; i++)
 {
    s += 'function x' + i + '() { x' + i + '(); }\n';
}
evaluate(s);
var g = newGlobal({newCompartment: true});
(new Debugger).addDebuggee(g);
g.offThreadCompileToStencil('debugger;',{});
var stencil = finishOffThreadStencil();
g.evalStencil(stencil);
