gczeal(0);

var ex;
function makeExtensibleStrFrom() {
    strstrstr;
}
a = makeExtensibleStrFrom;
b = newDependentString(a, 0, 50, { tenured: false })
var exc;
try {
    c = newDependentString(b, 0, { tenured: true })
} catch (e) {
    exc = e;
}
assertEq(Boolean(exc), true, "b and c required to be in different heaps but are the same");
