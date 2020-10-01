// |jit-test| skip-if: helperThreadCount() === 0
var src = "function foo() {}"
src += "foo(";
for (var i = 0; i < 50000; i++) {
    src += i + ",";
}
src += "1);\n"

evalInWorker(src);
