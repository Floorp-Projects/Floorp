
function buildWithHoles() {
    var a = new Array(5);
    for (var cnt = 0; cnt < a.length; cnt+=2) {
        a[cnt] = cnt;
    }
    var b = [0,1,2,3,4];
    var p = new ParallelArray(a);
    // check no holes
    assertEq(Object.keys(p).join(","), Object.keys(b).join(","));
}

buildWithHoles();
