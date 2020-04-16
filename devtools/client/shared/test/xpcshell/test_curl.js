/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests utility functions contained in `source-utils.js`
 */

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const curl = require("devtools/client/shared/curl");
const Curl = curl.Curl;
const CurlUtils = curl.CurlUtils;

const Services = require("Services");

// Test `Curl.generateCommand` headers forwarding/filtering
add_task(async function() {
  const request = {
    url: "https://example.com/form/",
    method: "GET",
    headers: [
      { name: "Host", value: "example.com" },
      {
        name: "User-Agent",
        value:
          "Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0",
      },
      { name: "Accept", value: "*/*" },
      { name: "Accept-Language", value: "en-US,en;q=0.5" },
      { name: "Accept-Encoding", value: "gzip, deflate, br" },
      { name: "Origin", value: "https://example.com" },
      { name: "Connection", value: "keep-alive" },
      { name: "Referer", value: "https://example.com/home/" },
      { name: "Content-Type", value: "text/plain" },
    ],
    httpVersion: "HTTP/2.0",
  };

  const cmd = Curl.generateCommand(request);
  const curlParams = parseCurl(cmd);

  ok(
    !headerTypeInParams(curlParams, "Host"),
    "host header ignored - to be generated from url"
  );
  ok(
    exactHeaderInParams(curlParams, "Accept: */*"),
    "accept header present in curl command"
  );
  ok(
    exactHeaderInParams(
      curlParams,
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0"
    ),
    "user-agent header present in curl command"
  );
  ok(
    exactHeaderInParams(curlParams, "Accept-Language: en-US,en;q=0.5"),
    "accept-language header present in curl output"
  );
  ok(
    !headerTypeInParams(curlParams, "Accept-Encoding") &&
      inParams(curlParams, "--compressed"),
    '"--compressed" param replaced accept-encoding header'
  );
  ok(
    exactHeaderInParams(curlParams, "Origin: https://example.com"),
    "origin header present in curl output"
  );
  ok(
    exactHeaderInParams(curlParams, "Connection: keep-alive"),
    "connection header present in curl output"
  );
  ok(
    exactHeaderInParams(curlParams, "Referer: https://example.com/home/"),
    "referer header present in curl output"
  );
  ok(
    exactHeaderInParams(curlParams, "Content-Type: text/plain"),
    "content-type header present in curl output"
  );
  ok(!inParams(curlParams, "--data"), "no data param in GET curl output");
  ok(
    !inParams(curlParams, "--data-raw"),
    "no raw data param in GET curl output"
  );
});

// Test `Curl.generateCommand` URL glob handling
add_task(async function() {
  let request = {
    url: "https://example.com/",
    method: "GET",
    headers: [],
    httpVersion: "HTTP/2.0",
  };

  let cmd = Curl.generateCommand(request);
  let curlParams = parseCurl(cmd);

  ok(
    !inParams(curlParams, "--globoff"),
    "no globoff param in curl output when not needed"
  );

  request = {
    url: "https://example.com/[]",
    method: "GET",
    headers: [],
    httpVersion: "HTTP/2.0",
  };

  cmd = Curl.generateCommand(request);
  curlParams = parseCurl(cmd);

  ok(
    inParams(curlParams, "--globoff"),
    "globoff param present in curl output when needed"
  );
});

// Test `Curl.generateCommand` data POSTing
add_task(async function() {
  const request = {
    url: "https://example.com/form/",
    method: "POST",
    headers: [
      { name: "Content-Length", value: "1000" },
      { name: "Content-Type", value: "text/plain" },
    ],
    httpVersion: "HTTP/2.0",
    postDataText: "A piece of plain payload text",
  };

  const cmd = Curl.generateCommand(request);
  const curlParams = parseCurl(cmd);

  ok(
    !headerTypeInParams(curlParams, "Content-Length"),
    "content-length header ignored - curl generates new one"
  );
  ok(
    exactHeaderInParams(curlParams, "Content-Type: text/plain"),
    "content-type header present in curl output"
  );
  ok(
    inParams(curlParams, "--data-raw"),
    '"--data-raw" param present in curl output'
  );
  ok(
    inParams(curlParams, `--data-raw ${quote(request.postDataText)}`),
    "proper payload data present in output"
  );
});

// Test `Curl.generateCommand` multipart data POSTing
add_task(async function() {
  const boundary = "----------14808";
  const request = {
    url: "https://example.com/form/",
    method: "POST",
    headers: [
      {
        name: "Content-Type",
        value: `multipart/form-data; boundary=${boundary}`,
      },
    ],
    httpVersion: "HTTP/2.0",
    postDataText: [
      `--${boundary}`,
      'Content-Disposition: form-data; name="field_one"',
      "",
      "value_one",
      `--${boundary}`,
      'Content-Disposition: form-data; name="field_two"',
      "",
      "value two",
      `--${boundary}--`,
      "",
    ].join("\r\n"),
  };

  const cmd = Curl.generateCommand(request);

  // Check content type
  const contentTypePos = cmd.indexOf(headerParamPrefix("Content-Type"));
  const contentTypeParam = headerParam(
    `Content-Type: multipart/form-data; boundary=${boundary}`
  );
  ok(contentTypePos !== -1, "content type header present in curl output");
  equal(
    cmd.substr(contentTypePos, contentTypeParam.length),
    contentTypeParam,
    "proper content type header present in curl output"
  );

  // Check binary data
  const dataBinaryPos = cmd.indexOf("--data-binary");
  const dataBinaryParam = `--data-binary ${isWin() ? "" : "$"}${escapeNewline(
    quote(request.postDataText)
  )}`;
  ok(dataBinaryPos !== -1, "--data-binary param present in curl output");
  equal(
    cmd.substr(dataBinaryPos, dataBinaryParam.length),
    dataBinaryParam,
    "proper multipart data present in curl output"
  );
});

// Test `CurlUtils.removeBinaryDataFromMultipartText` doesn't change text data
add_task(async function() {
  const boundary = "----------14808";
  const postTextLines = [
    `--${boundary}`,
    'Content-Disposition: form-data; name="field_one"',
    "",
    "value_one",
    `--${boundary}`,
    'Content-Disposition: form-data; name="field_two"',
    "",
    "value two",
    `--${boundary}--`,
    "",
  ];

  const cleanedText = CurlUtils.removeBinaryDataFromMultipartText(
    postTextLines.join("\r\n"),
    boundary
  );
  equal(
    cleanedText,
    postTextLines.join("\r\n"),
    "proper non-binary multipart text unchanged"
  );
});

// Test `CurlUtils.removeBinaryDataFromMultipartText` removes binary data
add_task(async function() {
  const boundary = "----------14808";
  const postTextLines = [
    `--${boundary}`,
    'Content-Disposition: form-data; name="field_one"',
    "",
    "value_one",
    `--${boundary}`,
    'Content-Disposition: form-data; name="field_two"; filename="file_field_two.txt"',
    "",
    "file content",
    `--${boundary}--`,
    "",
  ];

  const cleanedText = CurlUtils.removeBinaryDataFromMultipartText(
    postTextLines.join("\r\n"),
    boundary
  );
  postTextLines.splice(7, 1);
  equal(
    cleanedText,
    postTextLines.join("\r\n"),
    "file content removed from multipart text"
  );
});

function isWin() {
  return Services.appinfo.OS === "WINNT";
}

const QUOTE = isWin() ? '"' : "'";

// Quote a string, escape the quotes inside the string
function quote(str) {
  let escaped;
  if (isWin()) {
    escaped = str.replace(new RegExp(QUOTE, "g"), `${QUOTE}${QUOTE}`);
  } else {
    escaped = str.replace(new RegExp(QUOTE, "g"), `\\${QUOTE}`);
  }
  return QUOTE + escaped + QUOTE;
}

function escapeNewline(txt) {
  if (isWin()) {
    // Add `"` to close quote, then escape newline outside of quote, then start new quote
    return txt.replace(/[\r\n]{1,2}/g, '"^$&$&"');
  }
  return txt.replace(/\r/g, "\\r").replace(/\n/g, "\\n");
}

// Header param is formatted as -H "Header: value" or -H 'Header: value'
function headerParam(h) {
  return "-H " + quote(h);
}

// Header param prefix is formatted as `-H "HeaderName` or `-H 'HeaderName`
function headerParamPrefix(headerName) {
  return `-H ${QUOTE}${headerName}`;
}

// If any params startswith `-H "HeaderName` or `-H 'HeaderName`
function headerTypeInParams(curlParams, headerName) {
  return curlParams.some(param =>
    param.toLowerCase().startsWith(headerParamPrefix(headerName).toLowerCase())
  );
}

function exactHeaderInParams(curlParams, header) {
  return curlParams.some(param => param === headerParam(header));
}

function inParams(curlParams, param) {
  return curlParams.some(p => p.startsWith(param));
}

// Parse complete curl command to array of params. Can be applied to simple headers/data,
// but will not on WIN with sophisticated values of --data-binary with e.g. escaped quotes
function parseCurl(curlCmd) {
  // This monster regexp parses the command line into an array of arguments,
  // recognizing quoted args with matching quotes and escaped quotes inside:
  // [ "curl 'url'", "--standalone-arg", "-arg-with-quoted-string 'value\'s'" ]
  const matchRe = /[-A-Za-z1-9]+(?: \$?([\"'])(?:\\\1|.)*?\1)?/g;
  return curlCmd.match(matchRe);
}
