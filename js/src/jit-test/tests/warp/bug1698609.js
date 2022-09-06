// |jit-test| skip-if: helperThreadCount() === 0; --code-coverage
// Note: --code-coverage is a hack here to disable lazy parsing.

var src = "function foo(x) { return /abc/.test(x) }";
var job = offThreadCompileToStencil(src);
var stencil = finishOffThreadStencil(job);
var re = evalStencil(stencil);

for (var i = 0; i < 200; i++) {
    foo("abc");
}
