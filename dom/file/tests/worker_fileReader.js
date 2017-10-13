importScripts('common_fileReader.js');

function ok(a, msg) {
  postMessage({type:'check', msg, status: !!a});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

onmessage = event => {
  runTests(event.data).then(() => {
    postMessage({ type: 'finish' });
  });
};
