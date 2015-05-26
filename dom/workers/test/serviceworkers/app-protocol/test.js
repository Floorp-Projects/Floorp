function ok(aCondition, aMessage) {
  if (aCondition) {
    alert('OK: ' + aMessage);
  } else {
    alert('KO: ' + aMessage);
  }
}

function ready() {
  alert('READY');
}

function done() {
  alert('DONE');
}

function testFetchAppResource(aExpectedResponse) {
  return fetch('foo.txt').then(res => {
    ok(true, 'fetch should resolve');
    if (res.type == 'error') {
      ok(false, 'fetch failed');
    }
    ok(res.status == 200, 'status should be 200');
    ok(res.statusText == 'OK', 'statusText should be OK');
    return res.text().then(body => {
      ok(body == aExpectedResponse, 'body should match');
    });
  });
}
