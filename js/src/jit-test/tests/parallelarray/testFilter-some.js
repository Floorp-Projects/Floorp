
function testFilterSome() {
    var evenBelowThree = function (i) { return ((i%2) === 0) && (i < 3); };
    var p = new ParallelArray([0,1,2,3,4]);
    var r = p.filter(evenBelowThree);
    assertEq(r.toString(), [0,2].join(","));
}

testFilterSome();
