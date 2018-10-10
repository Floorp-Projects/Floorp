// |jit-test| skip-if: helperThreadCount() === 0

evalInWorker(`
var gTestcases = new Array();
typeof document != "object" || !document.location.href.match(/jsreftest.html/);
gczeal(4, 10);
f = ([a = class target extends b {}, b] = [void 0]) => {};
f()
`)
