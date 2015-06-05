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

function testFetchAppResource(aUrl,
                              aExpectedResponse,
                              aExpectedContentType) {
  return fetch(aUrl).then(res => {
    ok(true, 'fetch should resolve');
    if (res.type == 'error') {
      ok(false, 'fetch failed');
    }
    ok(res.status == 200, 'status should be 200');
    ok(res.statusText == 'OK', 'statusText should be OK');
    if (aExpectedContentType) {
      var headers = res.headers.getAll('Content-Type');
      ok(headers.length, "Headers length");
      var contentType = res.headers.get('Content-Type');
      ok(contentType == aExpectedContentType, ('content type ' +
         contentType + ' should match with ' + aExpectedContentType));
    }
    return res.text().then(body => {
      ok(body == aExpectedResponse, 'body ' + body +
         ' should match with ' + aExpectedResponse);
    });
  });
}
