function TestCase(n, d, e, a) {
    this.name = n
    this.description = d
}
function reportCompare(expected, actual, description) {
    new TestCase
}
var actual = '';
var expect = '';
for (var i = 0; i < 2; ++i) reportCompare(expect, actual, ': 2');
try {
    ({
        valueOf: gc
    } - [])
} catch (prop) {}
function addThis() { return reportCompare(expect, actual, 'ok'); }
Object.defineProperty(Object.prototype, "name", {
    set: function (newValue) {}
});
addThis()
