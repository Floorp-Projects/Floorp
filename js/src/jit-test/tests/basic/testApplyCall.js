function testApplyCallHelper(f) {
    var r = [];
    for (var i = 0; i < 10; ++i) f.call();
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2,3);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2,3]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2,3,4);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2,3,4]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2,3,4,5);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2,3,4,5])
    r.push(x);
    return(r.join(","));
}
function testApplyCall() {
    var r = testApplyCallHelper(function (a0,a1,a2,a3,a4,a5,a6,a7) { x = [a0,a1,a2,a3,a4,a5,a6,a7]; });
    r += testApplyCallHelper(function (a0,a1,a2,a3,a4,a5,a6,a7) { x = [a0,a1,a2,a3,a4,a5,a6,a7]; });
    return r;
}

assertEq(testApplyCall(), ",,,,,,,,,,,,,,,,,,,,,,,,0,,,,,,,,0,,,,,,,,0,1,,,,,,,0,1,,,,,,,0,1,2,,,,,,0,1,2,,,,,,0,1,2,3,,,,,0,1,2,3,,,,,0,1,2,3,4,,,,0,1,2,3,4,,,,0,1,2,3,4,5,,,0,1,2,3,4,5,," +
",,,,,,,,,,,,,,,,,,,,,,,,0,,,,,,,,0,,,,,,,,0,1,,,,,,,0,1,,,,,,,0,1,2,,,,,,0,1,2,,,,,,0,1,2,3,,,,,0,1,2,3,,,,,0,1,2,3,4,,,,0,1,2,3,4,,,,0,1,2,3,4,5,,,0,1,2,3,4,5,,");
