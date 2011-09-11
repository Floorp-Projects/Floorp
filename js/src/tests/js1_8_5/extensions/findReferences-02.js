// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jim Blandy

if (typeof findReferences == "function") {
    (function f() {
         assertEq(referencesVia(arguments, 'callee', f), true);
     })();

    var o = ({});

    function returnFlat(x) { return function flat() { return x; }; }
    assertEq(referencesVia(returnFlat(o), 'upvars[0]', o), true);

    function returnHeavy(y) { eval(''); return function heavy() { return y; }; }
    assertEq(referencesVia(returnHeavy(o), 'parent; y', o), true);
    assertEq(referencesVia(returnHeavy(o), 'parent; parent', this), true);

    function returnBlock(z) { eval(''); let(w = z) { return function block() { return w; }; }; }
    assertEq(referencesVia(returnBlock(o), 'parent; w', o), true);

    function returnWithObj(v) { with(v) return function withObj() { return u; }; }
    assertEq(referencesVia(returnWithObj(o), 'parent; type; type_proto', o), true);

    reportCompare(true, true);
} else {
    reportCompare(true, true, "test skipped: findReferences is not a function");
}
