/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Dave Herman
 */

function earlyError(src) {
    var threw;
    try {
        eval(src);
        threw = false;
    } catch (expected) {
        threw = true;
    }
    assertEq(threw, true);
}

earlyError("'use strict'; let (x) 42;");
earlyError("function f() { 'use strict'; let (x) 42;");
earlyError("'use strict'; function id(x) { return x } let (a=1) a ? f : x++(42);");
earlyError("function id(x) { return x } function f() { 'use strict'; let (a=1) a ? f : x++(42); }");
earlyError("'use strict'; let (x=2, y=3) x=3, y=13");
earlyError("function f() { 'use strict'; let (x=2, y=3) x=3, y=13 }");

x = "global";
(let (x=2, y=3) x=3, y=13);
assertEq(x, "global");
assertEq(y, 13);

// https://bugzilla.mozilla.org/show_bug.cgi?id=569464#c12
g = (let (x=7) x*x for each (x in [1,2,3]));
for (let y in g) {
    assertEq(y, 49);
}

reportCompare(0, 0, "In strict mode, let expression statements are disallowed.");
