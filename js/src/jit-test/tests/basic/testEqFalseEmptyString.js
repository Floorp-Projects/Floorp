function testEqFalseEmptyString() {
    var x = [];
    for (var i=0;i<5;++i) x.push(false == "");
    return x.join(",");
}
assertEq(testEqFalseEmptyString(), "true,true,true,true,true");
