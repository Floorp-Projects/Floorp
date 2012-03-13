
function combineParallelArray() {
    var p = new ParallelArray([0,1,2,3,4]);
    var a = new ParallelArray([1,2,3,4,5]);
    var r = p.combine(function (idx) { return this[idx] + a[idx];});
    assertEq(r.toString(), [1,3,5,7,9].join(","));
}

combineParallelArray();

