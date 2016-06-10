var port;

function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  port.postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  port.postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function isnot(a, b, msg) {
  dump("ISNOT: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  port.postMessage({type: 'status', status: a != b, msg: a + " != " + b + ": " + msg });
}

importScripts('test_performance_user_timing.js');

onconnect = function(evt) {
  port = evt.ports[0];

  for (var i = 0; i < steps.length; ++i) {
    performance.clearMarks();
    performance.clearMeasures();
    steps[i]();
  }

  port.postMessage({type: 'finish'});
}
