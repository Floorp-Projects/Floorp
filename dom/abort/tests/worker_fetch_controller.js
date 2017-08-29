function testWorkerAbortedFetch() {
  var fc = new FetchController();
  fc.abort();

  fetch('slow.sjs', { signal: fc.signal }).then(() => {
    postMessage(false);
  }, e => {
    postMessage(e.name == "AbortError");
  });
}

function testWorkerFetchAndAbort() {
  var fc = new FetchController();

  var p = fetch('slow.sjs', { signal: fc.signal });
  fc.abort();

  p.then(() => {
    postMessage(false);
  }, e => {
    postMessage(e.name == "AbortError");
  });
}

onmessage = function(e) {
  self[e.data]();
}
