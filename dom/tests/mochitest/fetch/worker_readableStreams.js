importScripts('common_readableStreams.js');

function info(message) {
  postMessage({type: 'info', message });
}

function ok(a, message) {
  postMessage({type: 'test', test: !!a, message });
}

function is(a, b, message) {
  ok(a === b, message);
}

function next() {
  postMessage({type: 'done'});
}

onmessage = function(e) {
  self[e.data](SAME_COMPARTMENT);
}
