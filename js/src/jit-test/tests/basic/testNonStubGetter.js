function testNonStubGetter() {
    let ([] = false) { (this.watch("x", function(p, o, n) { return /a/g.exec(p, o, n); })); };
    (function () { (eval("(function(){for each (x in [1, 2, 2]);});"))(); })();
    this.unwatch("x");
    return "ok";
}
assertEq(testNonStubGetter(), "ok");
