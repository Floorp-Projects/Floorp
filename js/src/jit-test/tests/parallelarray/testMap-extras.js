
function testMapExtras() {
    var p = new ParallelArray([0,1,2,3,4]);
    var m = p.map(function (v, e) { return v+e; }, [1,2,3,4,5]);
    assertEq(m.toString(), [1,3,5,7,9].join(","));
}

testMapExtras();
