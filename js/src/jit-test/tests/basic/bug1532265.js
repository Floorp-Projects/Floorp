// |jit-test| allow-oom; skip-if: !('oomTest' in this)

ignoreUnhandledRejections();

var v = {}
async function f() {
    // Increasing level of stack size during await to make OOM more likely when
    // saving the stack state.
              [await v];
             [[await v]];
            [[[await v]]];
           [[[[await v]]]];
          [[[[[await v]]]]];
         [[[[[[await v]]]]]];
        [[[[[[[await v]]]]]]];
       [[[[[[[[await v]]]]]]]];
      [[[[[[[[[await v]]]]]]]]];
     [[[[[[[[[[await v]]]]]]]]]];
}

oomTest(function() {
    for (var i = 0; i < 8; ++i) {
        f();
    }

    // Drain all jobs, ignoring any OOM errors.
    while (true) {
        try {
            drainJobQueue();
            break;
        } catch {}
    }
});
