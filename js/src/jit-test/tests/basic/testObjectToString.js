function testObjectToString() {
    var o = {toString: () => "foo"};
    var s = "";
    for (var i = 0; i < 10; i++)
        s += o;
    return s;
}
assertEq(testObjectToString(), "foofoofoofoofoofoofoofoofoofoo");
