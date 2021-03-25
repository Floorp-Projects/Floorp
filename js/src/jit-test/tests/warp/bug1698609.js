// |jit-test| skip-if: helperThreadCount() === 0; --off-thread-parse-global; --code-coverage
// Note: --code-coverage is a hack here to disable lazy parsing.

var src = "function foo(x) { return /abc/.test(x) }";
var script = offThreadCompileScript(src);
var re = runOffThreadScript(script);

for (var i = 0; i < 200; i++) {
    foo("abc");
}
