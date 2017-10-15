function testWorkerAbortedFetch() {
  var ac = new AbortController();
  ac.abort();

  fetch("slow.sjs", { signal: ac.signal }).then(() => {
    postMessage(false);
  }, e => {
    postMessage(e.name == "AbortError");
  });
}

function testWorkerFetchAndAbort() {
  var ac = new AbortController();

  var p = fetch("slow.sjs", { signal: ac.signal });
  ac.abort();

  p.then(() => {
    postMessage(false);
  }, e => {
    postMessage(e.name == "AbortError");
  });
}

self.onmessage = function(e) {
  self[e.data]();
};
