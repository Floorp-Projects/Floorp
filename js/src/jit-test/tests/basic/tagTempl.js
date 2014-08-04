/*
 * Tests tagged template string functionality.
 */

function check(actual, expected) {
    assertEq(actual.length, expected.length);
    for (var i = 0; i < expected.length; i++)
       assertEq(actual[i], expected[i]);
}

function func(a) { return a; }

function csoLoop() {
    var cso = [];
    for (var index = 0; index < 2000; index++) {
        cso[index] = func`hey${4}there`;
        if (index > 0)
            assertEq(cso[index - 1], cso[index]);
    }
}

// Tests baseline and ion functionality.
csoLoop();

// Tests off thread compilation
if (helperThreadCount() !== 0) {
    offThreadCompileScript("(x=>x)`abc`");
    a = runOffThreadScript();
    check(a, ["abc"]);
    check(a.raw, ["abc"]);
    assertEq(a === a.raw, false);
    assertEq(Object.isFrozen(a), true);
    assertEq(Object.isFrozen(a.raw), true);
}
