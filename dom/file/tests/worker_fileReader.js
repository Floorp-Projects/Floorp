importScripts('common_fileReader.js');

function ok(a, msg) {
  postMessage({type:'check', msg, status: !!a});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

onmessage = event => {
  let p;

  if (event.data.tests == 'basic') {
    p = runBasicTests(event.data.data);
  } else if (event.data.tests == 'encoding') {
    p = runEncodingTests(event.data.data);
  } else if (event.data.tests == 'twice') {
    p = runTwiceTests(event.data.data);
  } else if (event.data.tests == 'other') {
    p = runOtherTests(event.data.data);
  } else {
    postMessage({type: 'error'});
    return;
  }

  p.then(() => {
    postMessage({ type: 'finish' });
  });
};
