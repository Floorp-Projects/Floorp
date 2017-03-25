/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests Curl Utils functionality.
 */

const { Curl, CurlUtils } = require("devtools/client/shared/curl");

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CURL_UTILS_URL);
  info("Starting test... ");

  let { gStore, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/actions/index");
  let { getSortedRequests } = windowRequire("devtools/client/netmonitor/selectors/index");
  let { getLongString } = windowRequire("devtools/client/netmonitor/utils/client");

  gStore.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1, 3);
  yield ContentTask.spawn(tab.linkedBrowser, SIMPLE_SJS, function* (url) {
    content.wrappedJSObject.performRequests(url);
  });
  yield wait;

  let requests = {
    get: getSortedRequests(gStore.getState()).get(0),
    post: getSortedRequests(gStore.getState()).get(1),
    multipart: getSortedRequests(gStore.getState()).get(2),
    multipartForm: getSortedRequests(gStore.getState()).get(3),
  };

  let data = yield createCurlData(requests.get, getLongString);
  testFindHeader(data);

  data = yield createCurlData(requests.post, getLongString);
  testIsUrlEncodedRequest(data);
  testWritePostDataTextParams(data);
  testDataArgumentOnGeneratedCommand(data);

  data = yield createCurlData(requests.multipart, getLongString);
  testIsMultipartRequest(data);
  testGetMultipartBoundary(data);
  testMultiPartHeaders(data);
  testRemoveBinaryDataFromMultipartText(data);

  data = yield createCurlData(requests.multipartForm, getLongString);
  testMultiPartHeaders(data);

  testGetHeadersFromMultipartText({
    postDataText: "Content-Type: text/plain\r\n\r\n",
  });

  if (Services.appinfo.OS != "WINNT") {
    testEscapeStringPosix();
  } else {
    testEscapeStringWin();
  }

  yield teardown(monitor);
});

function testIsUrlEncodedRequest(data) {
  let isUrlEncoded = CurlUtils.isUrlEncodedRequest(data);
  ok(isUrlEncoded, "Should return true for url encoded requests.");
}

function testIsMultipartRequest(data) {
  let isMultipart = CurlUtils.isMultipartRequest(data);
  ok(isMultipart, "Should return true for multipart/form-data requests.");
}

function testFindHeader(data) {
  let headers = data.headers;
  let hostName = CurlUtils.findHeader(headers, "Host");
  let requestedWithLowerCased = CurlUtils.findHeader(headers, "x-requested-with");
  let doesNotExist = CurlUtils.findHeader(headers, "X-Does-Not-Exist");

  is(hostName, "example.com",
    "Header with name 'Host' should be found in the request array.");
  is(requestedWithLowerCased, "XMLHttpRequest",
    "The search should be case insensitive.");
  is(doesNotExist, null,
    "Should return null when a header is not found.");
}

function testMultiPartHeaders(data) {
  let headers = data.headers;
  let contentType = CurlUtils.findHeader(headers, "Content-Type");

  ok(contentType.startsWith("multipart/form-data; boundary="),
     "Multi-part content type header is present in headers array");
}

function testWritePostDataTextParams(data) {
  let params = CurlUtils.writePostDataTextParams(data.postDataText);
  is(params, "param1=value1&param2=value2&param3=value3",
    "Should return a serialized representation of the request parameters");
}

function testDataArgumentOnGeneratedCommand(data) {
  let curlCommand = Curl.generateCommand(data);
  ok(curlCommand.includes("--data"),
    "Should return a curl command with --data");
}

function testGetMultipartBoundary(data) {
  let boundary = CurlUtils.getMultipartBoundary(data);
  ok(/-{3,}\w+/.test(boundary),
    "A boundary string should be found in a multipart request.");
}

function testRemoveBinaryDataFromMultipartText(data) {
  let generatedBoundary = CurlUtils.getMultipartBoundary(data);
  let text = data.postDataText;
  let binaryRemoved =
    CurlUtils.removeBinaryDataFromMultipartText(text, generatedBoundary);
  let boundary = "--" + generatedBoundary;

  const EXPECTED_POSIX_RESULT = [
    "$'",
    boundary,
    "\\r\\n\\r\\n",
    "Content-Disposition: form-data; name=\"param1\"",
    "\\r\\n\\r\\n",
    "value1",
    "\\r\\n",
    boundary,
    "\\r\\n\\r\\n",
    "Content-Disposition: form-data; name=\"file\"; filename=\"filename.png\"",
    "\\r\\n",
    "Content-Type: image/png",
    "\\r\\n\\r\\n",
    boundary + "--",
    "\\r\\n",
    "'"
  ].join("");

  const EXPECTED_WIN_RESULT = [
    '"' + boundary + '"^',
    "\u000d\u000A\u000d\u000A",
    '"Content-Disposition: form-data; name=""param1"""^',
    "\u000d\u000A\u000d\u000A",
    '"value1"^',
    "\u000d\u000A",
    '"' + boundary + '"^',
    "\u000d\u000A\u000d\u000A",
    '"Content-Disposition: form-data; name=""file""; filename=""filename.png"""^',
    "\u000d\u000A",
    '"Content-Type: image/png"^',
    "\u000d\u000A\u000d\u000A",
    '"' + boundary + '--"^',
    "\u000d\u000A",
    '""'
  ].join("");

  if (Services.appinfo.OS != "WINNT") {
    is(CurlUtils.escapeStringPosix(binaryRemoved), EXPECTED_POSIX_RESULT,
      "The mulitpart request payload should not contain binary data.");
  } else {
    is(CurlUtils.escapeStringWin(binaryRemoved), EXPECTED_WIN_RESULT,
      "WinNT: The mulitpart request payload should not contain binary data.");
  }
}

function testGetHeadersFromMultipartText(data) {
  let headers = CurlUtils.getHeadersFromMultipartText(data.postDataText);

  ok(Array.isArray(headers), "Should return an array.");
  ok(headers.length > 0, "There should exist at least one request header.");
  is(headers[0].name, "Content-Type", "The first header name should be 'Content-Type'.");
}

function testEscapeStringPosix() {
  let surroundedWithQuotes = "A simple string";
  is(CurlUtils.escapeStringPosix(surroundedWithQuotes), "'A simple string'",
    "The string should be surrounded with single quotes.");

  let singleQuotes = "It's unusual to put crickets in your coffee.";
  is(CurlUtils.escapeStringPosix(singleQuotes),
    "$'It\\'s unusual to put crickets in your coffee.'",
    "Single quotes should be escaped.");

  let newLines = "Line 1\r\nLine 2\u000d\u000ALine3";
  is(CurlUtils.escapeStringPosix(newLines), "$'Line 1\\r\\nLine 2\\r\\nLine3'",
    "Newlines should be escaped.");

  let controlChars = "\u0007 \u0009 \u000C \u001B";
  is(CurlUtils.escapeStringPosix(controlChars), "$'\\x07 \\x09 \\x0c \\x1b'",
    "Control characters should be escaped.");

  let extendedAsciiChars = "æ ø ü ß ö é";
  is(CurlUtils.escapeStringPosix(extendedAsciiChars),
    "$'\\xc3\\xa6 \\xc3\\xb8 \\xc3\\xbc \\xc3\\x9f \\xc3\\xb6 \\xc3\\xa9'",
    "Character codes outside of the decimal range 32 - 126 should be escaped.");
}

function testEscapeStringWin() {
  let surroundedWithDoubleQuotes = "A simple string";
  is(CurlUtils.escapeStringWin(surroundedWithDoubleQuotes), '"A simple string"',
    "The string should be surrounded with double quotes.");

  let doubleQuotes = "Quote: \"Time is an illusion. Lunchtime doubly so.\"";
  is(CurlUtils.escapeStringWin(doubleQuotes),
    '"Quote: ""Time is an illusion. Lunchtime doubly so."""',
    "Double quotes should be escaped.");

  let percentSigns = "%AppData%";
  is(CurlUtils.escapeStringWin(percentSigns), '""%"AppData"%""',
    "Percent signs should be escaped.");

  let backslashes = "\\A simple string\\";
  is(CurlUtils.escapeStringWin(backslashes), '"\\\\A simple string\\\\"',
    "Backslashes should be escaped.");

  let newLines = "line1\r\nline2\r\nline3";
  is(CurlUtils.escapeStringWin(newLines),
    '"line1"^\u000d\u000A"line2"^\u000d\u000A"line3"',
    "Newlines should be escaped.");
}

function* createCurlData(selected, getLongString) {
  let { url, method, httpVersion } = selected;

  // Create a sanitized object for the Curl command generator.
  let data = {
    url,
    method,
    headers: [],
    httpVersion,
    postDataText: null
  };

  // Fetch header values.
  for (let { name, value } of selected.requestHeaders.headers) {
    let text = yield getLongString(value);
    data.headers.push({ name: name, value: text });
  }

  // Fetch the request payload.
  if (selected.requestPostData) {
    let postData = selected.requestPostData.postData.text;
    data.postDataText = yield getLongString(postData);
  }

  return data;
}
