ff = (function g() {
    for (var i=0; i<15; i++) {}
    return g;
});
assertEq(ff(), ff);
