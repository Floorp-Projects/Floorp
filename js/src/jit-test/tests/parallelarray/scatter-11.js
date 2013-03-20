load(libdir + "parallelarray-helpers.js");

// Test specific scatter implementation strategies, and compare them
// each against the sequential version.
//
// This is a reverse permutation that has a gap at the end.
// [A, B, ..., Y, Z] ==> [Z, Y, ..., B, A, 0]

function testDivideScatterVector() {
    var len = 1024;
    function add1(x) { return x+1; }
    function id(x) { return x; }
    var p = new ParallelArray(len, add1);
    var revidx = build(len, id).reverse();
    var p2 = new ParallelArray(revidx.map(add1).concat([0]));
    var modes = [["seq", ""],
                 ["par", "divide-scatter-vector"],
                 ["par", "divide-output-range"]];
    for (var i = 0; i < 3; i++) {
        var r = p.scatter(revidx, 0, undefined, len+1,
                          {mode: modes[i][0], strategy: modes[i][1],
                           expect: "success"});
      print(modes[i][1], r.buffer);
        assertEqParallelArray(r, p2);
    }
}

if (getBuildConfiguration().parallelJS) testDivideScatterVector();
