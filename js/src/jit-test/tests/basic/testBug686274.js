// Don't Baseline-compile the huge array literal with --baseline-eager
// as it's slow, especially in debug builds.
setJitCompilerOption("baseline.warmup.trigger", 2);

function f(N) {
    var body = "return [";
    for (var i = 0; i < N-1; i++)
        body += "1,";
    body += "2]";
    var f = new Function(body);
    var arr = f();
    assertEq(arr.length, N);
    assertEq(arr[N-1], 2);
}
f(1000000);
