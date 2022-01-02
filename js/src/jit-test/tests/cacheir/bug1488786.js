setJitCompilerOption('ion.forceinlineCaches', 1);

var A = Array(2**18);
A[0] = "A";
A[1] = "B";
A[2**14] = "C";
A[2**31-1] = "D";
A[-1] = "E";

function get_thee(a,x) {
    return a[x];
}

// Warmup IC
for (var i = 0; i < 30; i++) {
    get_thee(A, i % A.length);
}

// numberToDouble always returns a double-typed Number, so helps
// us ensure we're accessing the array with a double-typed Number index.
var y = numberToDouble(1);
var z = 2**31-1;
// Ensure we handle negative indices.
var a = -1;

function test() {
    for (var i = 0; i < 30; i++) {
        assertEq(get_thee(A,y), "B");
        assertEq(get_thee(A,z), "D");
        assertEq(get_thee(A,a), "E");
    }
}
test();

if (!('oomTest' in this))
    quit();

oomTest(test);
