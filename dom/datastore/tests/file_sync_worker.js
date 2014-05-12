function is(a, b, msg) {
  postMessage((a === b ? 'OK' : 'KO') + ' ' + msg)
}

function ok(a, msg) {
  postMessage((a ? 'OK' : 'KO')+ ' ' + msg)
}

function cbError() {
  postMessage('KO error');
}

function finish() {
  postMessage('DONE');
}

importScripts("file_sync_common.js");

runTest();
