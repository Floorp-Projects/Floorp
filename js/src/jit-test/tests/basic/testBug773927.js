function outer() {
    (function() {x})
    assertEq(((function() {return x}) for (x in [42])).next()(), "0");
    var x;
}
outer();
