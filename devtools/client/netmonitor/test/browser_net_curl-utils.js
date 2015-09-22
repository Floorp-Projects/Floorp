/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests Curl Utils functionality.
 */

function test() {
  initNetMonitor(CURL_UTILS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { NetMonitorView, gNetwork } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1, 3).then(() => {
      let requests = {
        get: RequestsMenu.getItemAtIndex(0),
        post: RequestsMenu.getItemAtIndex(1),
        multipart: RequestsMenu.getItemAtIndex(2),
        multipartForm: RequestsMenu.getItemAtIndex(3)
      };

      Task.spawn(function*() {
        yield createCurlData(requests.get.attachment, gNetwork).then((aData) => {
          test_findHeader(aData);
        });

        yield createCurlData(requests.post.attachment, gNetwork).then((aData) => {
          test_isUrlEncodedRequest(aData);
          test_writePostDataTextParams(aData);
        });

        yield createCurlData(requests.multipart.attachment, gNetwork).then((aData) => {
          test_isMultipartRequest(aData);
          test_getMultipartBoundary(aData);
          test_removeBinaryDataFromMultipartText(aData);
        });

        yield createCurlData(requests.multipartForm.attachment, gNetwork).then((aData) => {
          test_getHeadersFromMultipartText(aData);
        });

        if (Services.appinfo.OS != "WINNT") {
          test_escapeStringPosix();
        } else {
          test_escapeStringWin();
        }

        teardown(aMonitor).then(finish);
      });
    });

    aDebuggee.performRequests(SIMPLE_SJS);
  });
}

function test_isUrlEncodedRequest(aData) {
  let isUrlEncoded = CurlUtils.isUrlEncodedRequest(aData);
  ok(isUrlEncoded, "Should return true for url encoded requests.");
}

function test_isMultipartRequest(aData) {
  let isMultipart = CurlUtils.isMultipartRequest(aData);
  ok(isMultipart, "Should return true for multipart/form-data requests.");
}

function test_findHeader(aData) {
  let headers = aData.headers;
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

function test_writePostDataTextParams(aData) {
  let params = CurlUtils.writePostDataTextParams(aData.postDataText);
  is(params, "param1=value1&param2=value2&param3=value3",
    "Should return a serialized representation of the request parameters");
}

function test_getMultipartBoundary(aData) {
  let boundary = CurlUtils.getMultipartBoundary(aData);
  ok(/-{3,}\w+/.test(boundary),
    "A boundary string should be found in a multipart request.");
}

function test_removeBinaryDataFromMultipartText(aData) {
  let generatedBoundary = CurlUtils.getMultipartBoundary(aData);
  let text = aData.postDataText;
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
    generatedBoundary,
    "--\\r\\n",
    "'"
  ].join("");

  const EXPECTED_WIN_RESULT = [
    '"' + boundary + '"^',
    '\u000d\u000A\u000d\u000A',
    '"Content-Disposition: form-data; name=""param1"""^',
    '\u000d\u000A\u000d\u000A',
    '"value1"^',
    '\u000d\u000A',
    '"' + boundary + '"^',
    '\u000d\u000A\u000d\u000A',
    '"Content-Disposition: form-data; name=""file""; filename=""filename.png"""^',
    '\u000d\u000A',
    '"Content-Type: image/png"^',
    '\u000d\u000A\u000d\u000A',
    '"' + generatedBoundary + '--"^',
    '\u000d\u000A',
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

function test_getHeadersFromMultipartText(aData) {
  let headers = CurlUtils.getHeadersFromMultipartText(aData.postDataText);

  ok(Array.isArray(headers),
    "Should return an array.");
  ok(headers.length > 0,
    "There should exist at least one request header.");
  is(headers[0].name, "Content-Type",
    "The first header name should be 'Content-Type'.");
}

function test_escapeStringPosix() {
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

function test_escapeStringWin() {
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

function createCurlData(aSelected, aNetwork) {
  return Task.spawn(function*() {
    // Create a sanitized object for the Curl command generator.
    let data = {
      url: aSelected.url,
      method: aSelected.method,
      headers: [],
      httpVersion: aSelected.httpVersion,
      postDataText: null
    };

    // Fetch header values.
    for (let { name, value } of aSelected.requestHeaders.headers) {
      let text = yield aNetwork.getString(value);
      data.headers.push({ name: name, value: text });
    }

    // Fetch the request payload.
    if (aSelected.requestPostData) {
      let postData = aSelected.requestPostData.postData.text;
      data.postDataText = yield aNetwork.getString(postData);
    }

    return data;
  });
}