function testNonStubGetter() {
    let ([] = false) { (this.watch("x", /a/g)); };
    (function () { (eval("(function(){for each (x in [1, 2, 2]);});"))(); })();
    this.unwatch("x");
    return "ok";
}
assertEq(testNonStubGetter(), "ok");
