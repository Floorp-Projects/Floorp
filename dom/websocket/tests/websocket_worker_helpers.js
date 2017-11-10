function feedback() {
  postMessage({type: 'feedback', msg: "executing test: " + (current_test+1) + " of " + tests.length + " tests." });
}

function ok(status, msg) {
  postMessage({type: 'status', status: !!status, msg: msg});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function isnot(a, b, msg) {
  ok(a != b, msg);
}

function finish() {
  postMessage({type: 'finish'});
}
