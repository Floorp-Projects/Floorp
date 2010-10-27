function testApply() {
    var q = [];
    for (var i = 0; i < 10; ++i)
        Array.prototype.push.apply(q, [5]);
    return q.join(",");
}
assertEq(testApply(), "5,5,5,5,5,5,5,5,5,5");
