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

function testRedirectedResponse() {
  return testRedirectedResponseWorker("redirected", "IFRAMELOADED");
}

function testRedirectedHttpsResponse() {
  return testRedirectedResponseWorker("redirected-https", "HTTPSIFRAMELOADED");
}

function testCachedRedirectedResponse() {
  return testRedirectedResponseWorker("redirected-cached", "IFRAMELOADED");
}

function testCachedRedirectedHttpsResponse() {
  return testRedirectedResponseWorker("redirected-https-cached", "HTTPSIFRAMELOADED");
}

function testRedirectedResponseWorker(aFrameId, aAlert) {
  // Because of the CSP policies applied to privileged apps, we cannot run
  // inline script inside realindex.html, and loading a script from the app://
  // URI is also not an option, so we let the parent iframe which has access
  // to the SpecialPowers API use those privileges to access the document.
  var iframe = document.createElement("iframe");
  document.body.appendChild(iframe);
  iframe.src = aFrameId + ".html";
  iframe.id = aFrameId;
  return new Promise(resolve => {
    iframe.addEventListener("load", event => {
      alert(aAlert);
      resolve();
    }, false);
  });
}
