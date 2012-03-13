
function testFilterAll() {
    var even = function (i) { return (i%2 === 0); };
    var p = new ParallelArray([0,1,2,3,4]);
    var r = p.filter(even);
    assertEq(r.toString(), [0,2,4].join(","));
}

testFilterAll();
