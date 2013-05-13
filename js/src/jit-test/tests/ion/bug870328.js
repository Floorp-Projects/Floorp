var g = newGlobal("same-compartment");
try {
    evalcx("'use strict'; (function() { x = 33; })()", g);
    assertEq(0, 1);
} catch(e) {
    assertEq(e.toString().contains("variable x"), true);
}
