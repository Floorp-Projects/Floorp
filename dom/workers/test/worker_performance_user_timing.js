function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function isnot(a, b, msg) {
  dump("ISNOT: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a != b, msg: a + " != " + b + ": " + msg });
}

importScripts(['../../../dom/performance/tests/test_performance_user_timing.js']);

for (var i = 0; i < steps.length; ++i) {
  performance.clearMarks();
  performance.clearMeasures();
  steps[i]();
}

postMessage({type: 'finish'});
