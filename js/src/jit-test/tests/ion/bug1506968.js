// |jit-test| skip-if: typeof Intl === 'undefined'
function f(a, b) {
    a.formatToParts();
    a.format();
}
var a = new Intl.NumberFormat();
f(a, []);
try {
    f();
} catch (e) {}
f(a, []);
try {
    f();
} catch (e) {}
