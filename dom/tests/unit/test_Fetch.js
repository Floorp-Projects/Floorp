/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(['fetch']);
Cu.import("resource://testing-common/httpd.js");

const BinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

var server;

function getBaseUrl () {
  return "http://localhost:" + server.identity.primaryPort;
}

// a way to create some test defaults
function createTestData(testPath) {
  return {
    testPath: testPath,
    request: {
      headers: {},
      contentType: "application/json",
    },
    response: {
      headers: {},
      contentType: "application/json",
      body: "{\"Look\": \"Success!\"}",
      status: 200,
      statusText: "OK"
    },
  };
}

// read body and content type information from a request
function readDataFromRequest(aRequest) {
  let requestData = {};
  if (aRequest.method == "POST" || aRequest.method == "PUT") {
    if (aRequest.bodyInputStream) {
      let inputStream = new BinaryInputStream(aRequest.bodyInputStream);
      let bytes = [];
      let available;

      while ((available = inputStream.available()) > 0) {
        Array.prototype.push.apply(bytes, inputStream.readByteArray(available));
      }

      requestData.body = String.fromCharCode.apply(null, bytes);
      requestData.contentType = aRequest.getHeader("Content-Type");
    }
  }
  return requestData;
}

// write status information, content type information and body to a response
function writeDataToResponse(aData, aResponse) {
  aResponse.setStatusLine(null, aData.status, aData.statusText);
  aResponse.setHeader("Content-Type", aData.contentType);
  for (let header in aData.headers) {
    aResponse.setHeader(header, aData.headers[header]);
  }
  aResponse.write(aData.body);
}

// test some GET functionality
add_test(function test_GetData() {
  do_test_pending();

  let testData = createTestData("/getData");

  // set up some headers to test
  let headerNames = ["x-test-header", "x-other-test-header"];
  for (let headerName of headerNames) {
    testData.request.headers[headerName] = "test-value-for-" + headerName;
  }

  server.registerPathHandler(testData.testPath, function(aRequest, aResponse) {
    try {
      // check our request headers made it OK
      for (let headerName of headerNames) {
        Assert.equal(testData.request.headers[headerName],
                     aRequest.getHeader(headerName));
      }

      // send a response
      writeDataToResponse(testData.response, aResponse);
    } catch (e) {
      do_report_unexpected_exception(e);
    }
  });

  // fetch, via GET, with some request headers set
  fetch(getBaseUrl() + testData.testPath, {headers: testData.request.headers})
    .then(function(response){
    // check response looks as expected
    Assert.ok(response.ok);
    Assert.equal(response.status, testData.response.status);
    Assert.equal(response.statusText, testData.response.statusText);

    // check a response header looks OK:
    Assert.equal(response.headers.get("Content-Type"),
                 testData.response.contentType);

    // ... and again to check header names are case insensitive
    Assert.equal(response.headers.get("content-type"),
                 testData.response.contentType);

    // ensure response.text() returns a promise that resolves appropriately
    response.text().then(function(text) {
      Assert.equal(text, testData.response.body);
      do_test_finished();
      run_next_test();
    });
  }).catch(function(e){
    do_report_unexpected_exception(e);
    do_test_finished();
    run_next_test();
  });
});

// test a GET with no init
add_test(function test_GetDataNoInit() {
  do_test_pending();

  let testData = createTestData("/getData");

  server.registerPathHandler(testData.testPath, function(aRequest, aResponse) {
    try {
      // send a response
      writeDataToResponse(testData.response, aResponse);
    } catch (e) {
      do_report_unexpected_exception(e);
    }
  });

  fetch(getBaseUrl() + testData.testPath, {headers: testData.request.headers})
    .then(function(response){
    // check response looks as expected
    Assert.ok(response.ok);
    Assert.equal(response.status, testData.response.status);

    // ensure response.text() returns a promise that resolves appropriately
    response.text().then(function(text) {
      Assert.equal(text, testData.response.body);
      do_test_finished();
      run_next_test();
    });
  }).catch(function(e){
    do_report_unexpected_exception(e);
    do_test_finished();
    run_next_test();
  });
});

// test some error responses
add_test(function test_get40x() {
  do_test_pending();

  // prepare a response with some 40x error - a 404 will do
  let notFoundData = createTestData("/getNotFound");
  notFoundData.response.status = 404;
  notFoundData.response.statusText = "Not found";
  notFoundData.response.body = null;

  // No need to register a path handler - httpd will return 404 anyway.
  // Fetch, via GET, the resource that doesn't exist
  fetch(getBaseUrl() + notFoundData.testPath).then(function(response){
    Assert.equal(response.status, 404);
    do_test_finished();
    run_next_test();
  });
});

add_test(function test_get50x() {
  do_test_pending();

  // prepare a response with some server error - a 500 will do
  let serverErrorData = createTestData("/serverError");
  serverErrorData.response.status = 500;
  serverErrorData.response.statusText = "The server broke";
  serverErrorData.response.body = null;

  server.registerPathHandler(serverErrorData.testPath,
                             function(aRequest, aResponse) {
    try {
      // send the error response
      writeDataToResponse(serverErrorData.response, aResponse);
    } catch (e) {
      do_report_unexpected_exception(e);
    }
  });

  // fetch, via GET, the resource that creates a server error
  fetch(getBaseUrl() + serverErrorData.testPath).then(function(response){
    Assert.equal(response.status, 500);
    do_test_finished();
    run_next_test();
  });
});

// test a failure to connect
add_test(function test_getTestFailedConnect() {
  do_test_pending();
  // try a server that's not there
  fetch("http://localhost:4/should/fail").then(response => {
    do_throw("Request should not succeed");
  }).catch(err => {
    Assert.equal(true, err instanceof TypeError);
    do_test_finished();
    run_next_test();
  });
});

add_test(function test_mozError() {
  do_test_pending();
  // try a server that's not there
  fetch("http://localhost:4/should/fail", { mozErrors: true }).then(response => {
    do_throw("Request should not succeed");
  }).catch(err => {
    Assert.equal(err.result, Cr.NS_ERROR_CONNECTION_REFUSED);
    do_test_finished();
    run_next_test();
  });
});

add_test(function test_request_mozError() {
  do_test_pending();
  // try a server that's not there
  const r = new Request("http://localhost:4/should/fail", { mozErrors: true });
  fetch(r).then(response => {
    do_throw("Request should not succeed");
  }).catch(err => {
    Assert.equal(err.result, Cr.NS_ERROR_CONNECTION_REFUSED);
    do_test_finished();
    run_next_test();
  });
});

// test POSTing some JSON data
add_test(function test_PostJSONData() {
  do_test_pending();

  let testData = createTestData("/postJSONData");
  testData.request.body = "{\"foo\": \"bar\"}";

  server.registerPathHandler(testData.testPath, function(aRequest, aResponse) {
    try {
      let requestData = readDataFromRequest(aRequest);

      // Check the request body is OK
      Assert.equal(requestData.body, testData.request.body);

      // Check the content type is as expected
      Assert.equal(requestData.contentType, testData.request.contentType);

      writeDataToResponse(testData.response, aResponse);
    } catch (e) {
      Assert.ok(false);
    }
  });

  fetch(getBaseUrl() + testData.testPath, {
    method: "POST",
    body: testData.request.body,
    headers: {
      'Content-Type': 'application/json'
    }
  }).then(function(aResponse) {
    Assert.ok(aResponse.ok);
    Assert.equal(aResponse.status, testData.response.status);
    Assert.equal(aResponse.statusText, testData.response.statusText);

    do_test_finished();
    run_next_test();
  }).catch(function(e) {
    do_report_unexpected_exception(e);
    do_test_finished();
    run_next_test();
  });
});

// test POSTing some text
add_test(function test_PostTextData() {
  do_test_pending();

  let testData = createTestData("/postTextData");
  testData.request.body = "A plain text body";
  testData.request.contentType = "text/plain";
  let responseHeaderName = "some-response-header";
  testData.response.headers[responseHeaderName] = "some header value";

  server.registerPathHandler(testData.testPath, function(aRequest, aResponse) {
    try {
      let requestData = readDataFromRequest(aRequest);

      // Check the request body is OK
      Assert.equal(requestData.body, testData.request.body);

      // Check the content type is as expected
      Assert.equal(requestData.contentType, testData.request.contentType);

      writeDataToResponse(testData.response, aResponse);
    } catch (e) {
      Assert.ok(false);
    }
  });

  fetch(getBaseUrl() + testData.testPath, {
    method: "POST",
    body: testData.request.body,
    headers: {
      'Content-Type': testData.request.contentType
    }
  }).then(function(aResponse) {
    Assert.ok(aResponse.ok);
    Assert.equal(aResponse.status, testData.response.status);
    Assert.equal(aResponse.statusText, testData.response.statusText);

    // check the response header is set OK
    Assert.equal(aResponse.headers.get(responseHeaderName),
                 testData.response.headers[responseHeaderName]);

    do_test_finished();
    run_next_test();
  }).catch(function(e) {
    do_report_unexpected_exception(e);
    do_test_finished();
    run_next_test();
  });
});

function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  do_register_cleanup(function() {
    server.stop(function() { });
  });
}
