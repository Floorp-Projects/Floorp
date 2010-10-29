function testArrayPushPop() {
    var a = [], sum1 = 0, sum2 = 0;
    for (var i = 0; i < 10; ++i)
        sum1 += a.push(i);
    for (var i = 0; i < 10; ++i)
        sum2 += a.pop();
    a.push(sum1);
    a.push(sum2);
    return a.join(",");
}
assertEq(testArrayPushPop(), "55,45");
