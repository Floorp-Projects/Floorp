function testForEach() {
    var r;
    var a = ["zero", "one", "two", "three"];
    for (var i = 0; i < RUNLOOP; i++) {
        r = "";
        for each (var s in a)
            r += s + " ";
    }
    return r;
}
assertEq(testForEach(), "zero one two three ");
