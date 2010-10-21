const numFatArgs = Math.pow(2,19) - 1024;

function fun(x) {
    if (x <= 0)
        return 0;
    return fun(x-1);
}

function fatStack() {
    return fun(10000);
}

function assertRightFailure(e) {
    assertEq(e.toString() == "InternalError: script stack space quota is exhausted" ||
             e.toString() == "InternalError: too much recursion",
	     true);
}

exception = false;
try {
    fatStack.apply(null, new Array(numFatArgs));
} catch (e) {
    assertRightFailure(e);
    exception = true;
}
assertEq(exception, true);

// No more trace recursion w/ JM
checkStats({traceCompleted:0});
