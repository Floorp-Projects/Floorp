function matchInLoop() {
    var k = "hi";
    for (var i = 0; i < 10; i++) {
        var result = k.match(/hi/) != null;
    }
    return result;
}
assertEq(matchInLoop(), true);
