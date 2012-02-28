// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jim Blandy

if (typeof findReferences == "function") {
    (function f() {
         assertEq(referencesVia(arguments, 'callee', f), true);
     })();

    var o = ({});

    function returnHeavy(y) { eval(''); return function heavy() { return y; }; }
    assertEq(referencesVia(returnHeavy(o), 'fun_callscope; y', o), true);
    assertEq(referencesVia(returnHeavy(o), 'fun_callscope; shape; base; parent', this), true);

    function returnBlock(z) { eval(''); let(w = z) { return function block() { return w; }; }; }
    assertEq(referencesVia(returnBlock(o), 'fun_callscope; w', o), true);

    function returnWithObj(v) { with(v) return function withObj() { return u; }; }
    assertEq(referencesVia(returnWithObj(o), 'fun_callscope; type; type_proto', o), true);

    reportCompare(true, true);
} else {
    reportCompare(true, true, "test skipped: findReferences is not a function");
}
