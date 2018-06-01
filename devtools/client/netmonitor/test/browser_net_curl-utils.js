/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests Curl Utils functionality.
 */

const { Curl, CurlUtils } = require("devtools/client/shared/curl");

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CURL_UTILS_URL);
  info("Starting test... ");

  const { store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");
  const {
    getLongString,
    requestData,
  } = connector;

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 5);
  await ContentTask.spawn(tab.linkedBrowser, SIMPLE_SJS, async function(url) {
    content.wrappedJSObject.performRequests(url);
  });
  await wait;

  const requests = {
    get: getSortedRequests(store.getState()).get(0),
    post: getSortedRequests(store.getState()).get(1),
    patch: getSortedRequests(store.getState()).get(2),
    multipart: getSortedRequests(store.getState()).get(3),
    multipartForm: getSortedRequests(store.getState()).get(4),
  };

  let data = await createCurlData(requests.get, getLongString, requestData);
  testFindHeader(data);

  data = await createCurlData(requests.post, getLongString, requestData);
  testIsUrlEncodedRequest(data);
  testWritePostDataTextParams(data);
  testWriteEmptyPostDataTextParams(data);
  testDataArgumentOnGeneratedCommand(data);

  data = await createCurlData(requests.patch, getLongString, requestData);
  testWritePostDataTextParams(data);
  testDataArgumentOnGeneratedCommand(data);

  data = await createCurlData(requests.multipart, getLongString, requestData);
  testIsMultipartRequest(data);
  testGetMultipartBoundary(data);
  testMultiPartHeaders(data);
  testRemoveBinaryDataFromMultipartText(data);

  data = await createCurlData(requests.multipartForm, getLongString, requestData);
  testMultiPartHeaders(data);

  testGetHeadersFromMultipartText({
    postDataText: "Content-Type: text/plain\r\n\r\n",
  });

  if (Services.appinfo.OS != "WINNT") {
    testEscapeStringPosix();
  } else {
    testEscapeStringWin();
  }

  await teardown(monitor);
});

function testIsUrlEncodedRequest(data) {
  const isUrlEncoded = CurlUtils.isUrlEncodedRequest(data);
  ok(isUrlEncoded, "Should return true for url encoded requests.");
}

function testIsMultipartRequest(data) {
  const isMultipart = CurlUtils.isMultipartRequest(data);
  ok(isMultipart, "Should return true for multipart/form-data requests.");
}

function testFindHeader(data) {
  const headers = data.headers;
  const hostName = CurlUtils.findHeader(headers, "Host");
  const requestedWithLowerCased = CurlUtils.findHeader(headers, "x-requested-with");
  const doesNotExist = CurlUtils.findHeader(headers, "X-Does-Not-Exist");

  is(hostName, "example.com",
    "Header with name 'Host' should be found in the request array.");
  is(requestedWithLowerCased, "XMLHttpRequest",
    "The search should be case insensitive.");
  is(doesNotExist, null,
    "Should return null when a header is not found.");
}

function testMultiPartHeaders(data) {
  const headers = data.headers;
  const contentType = CurlUtils.findHeader(headers, "Content-Type");

  ok(contentType.startsWith("multipart/form-data; boundary="),
     "Multi-part content type header is present in headers array");
}

function testWritePostDataTextParams(data) {
  const params = CurlUtils.writePostDataTextParams(data.postDataText);
  is(params, "param1=value1&param2=value2&param3=value3",
    "Should return a serialized representation of the request parameters");
}

function testWriteEmptyPostDataTextParams(data) {
  const params = CurlUtils.writePostDataTextParams(null);
  is(params, "",
    "Should return a empty string when no parameters provided");
}

function testDataArgumentOnGeneratedCommand(data) {
  const curlCommand = Curl.generateCommand(data);
  ok(curlCommand.includes("--data"),
    "Should return a curl command with --data");
}

function testGetMultipartBoundary(data) {
  const boundary = CurlUtils.getMultipartBoundary(data);
  ok(/-{3,}\w+/.test(boundary),
    "A boundary string should be found in a multipart request.");
}

function testRemoveBinaryDataFromMultipartText(data) {
  const generatedBoundary = CurlUtils.getMultipartBoundary(data);
  const text = data.postDataText;
  const binaryRemoved =
    CurlUtils.removeBinaryDataFromMultipartText(text, generatedBoundary);
  const boundary = "--" + generatedBoundary;

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
  const headers = CurlUtils.getHeadersFromMultipartText(data.postDataText);

  ok(Array.isArray(headers), "Should return an array.");
  ok(headers.length > 0, "There should exist at least one request header.");
  is(headers[0].name, "Content-Type", "The first header name should be 'Content-Type'.");
}

function testEscapeStringPosix() {
  const surroundedWithQuotes = "A simple string";
  is(CurlUtils.escapeStringPosix(surroundedWithQuotes), "'A simple string'",
    "The string should be surrounded with single quotes.");

  const singleQuotes = "It's unusual to put crickets in your coffee.";
  is(CurlUtils.escapeStringPosix(singleQuotes),
    "$'It\\'s unusual to put crickets in your coffee.'",
    "Single quotes should be escaped.");

  const newLines = "Line 1\r\nLine 2\u000d\u000ALine3";
  is(CurlUtils.escapeStringPosix(newLines), "$'Line 1\\r\\nLine 2\\r\\nLine3'",
    "Newlines should be escaped.");

  const controlChars = "\u0007 \u0009 \u000C \u001B";
  is(CurlUtils.escapeStringPosix(controlChars), "$'\\x07 \\x09 \\x0c \\x1b'",
    "Control characters should be escaped.");

  const extendedAsciiChars = "æ ø ü ß ö é";
  is(CurlUtils.escapeStringPosix(extendedAsciiChars),
    "$'\\xc3\\xa6 \\xc3\\xb8 \\xc3\\xbc \\xc3\\x9f \\xc3\\xb6 \\xc3\\xa9'",
    "Character codes outside of the decimal range 32 - 126 should be escaped.");
}

function testEscapeStringWin() {
  const surroundedWithDoubleQuotes = "A simple string";
  is(CurlUtils.escapeStringWin(surroundedWithDoubleQuotes), '"A simple string"',
    "The string should be surrounded with double quotes.");

  const doubleQuotes = "Quote: \"Time is an illusion. Lunchtime doubly so.\"";
  is(CurlUtils.escapeStringWin(doubleQuotes),
    '"Quote: ""Time is an illusion. Lunchtime doubly so."""',
    "Double quotes should be escaped.");

  const percentSigns = "%AppData%";
  is(CurlUtils.escapeStringWin(percentSigns), '""%"AppData"%""',
    "Percent signs should be escaped.");

  const backslashes = "\\A simple string\\";
  is(CurlUtils.escapeStringWin(backslashes), '"\\\\A simple string\\\\"',
    "Backslashes should be escaped.");

  const newLines = "line1\r\nline2\r\nline3";
  is(CurlUtils.escapeStringWin(newLines),
    '"line1"^\u000d\u000A"line2"^\u000d\u000A"line3"',
    "Newlines should be escaped.");
}

async function createCurlData(selected, getLongString, requestData) {
  const { id, url, method, httpVersion } = selected;

  // Create a sanitized object for the Curl command generator.
  const data = {
    url,
    method,
    headers: [],
    httpVersion,
    postDataText: null
  };

  const requestHeaders = await requestData(id, "requestHeaders");
  // Fetch header values.
  for (const { name, value } of requestHeaders.headers) {
    const text = await getLongString(value);
    data.headers.push({ name: name, value: text });
  }

  const requestPostData = await requestData(id, "requestPostData");
  // Fetch the request payload.
  if (requestPostData) {
    const postData = requestPostData.postData.text;
    data.postDataText = await getLongString(postData);
  }

  return data;
}
