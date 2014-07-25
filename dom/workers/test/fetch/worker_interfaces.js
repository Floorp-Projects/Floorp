function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

onmessage = function() {
  ok(typeof Headers === "function", "Headers should be defined");
  ok(typeof Request === "function", "Request should be defined");
  ok(typeof Response === "function", "Response should be defined");
  ok(typeof fetch === "function", "fetch() should be defined");
  postMessage({ type: 'finish' });
}
