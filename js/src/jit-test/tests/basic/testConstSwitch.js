// No test case, just make sure this doesn't assert.
function testNegZero2() {
    var z = 0;
    for (let j = 0; j < 5; ++j) { ({p: (-z)}); }
}
testNegZero2();

function testConstSwitch() {
    var x;
    for (var j=0;j<5;++j) { switch(1.1) { case NaN: case 2: } x = 2; }
    return x;
}
assertEq(testConstSwitch(), 2);
