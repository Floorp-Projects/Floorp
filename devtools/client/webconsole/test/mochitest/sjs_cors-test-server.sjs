/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  const params = new Map(
    request.queryString
      .replace("?", "")
      .split("&")
      .map(s => s.split("="))
  );

  if (!params.has("corsErrorCategory")) {
    response.setStatusLine(request.httpVersion, 200, "Och Aye");
    setCacheHeaders(response);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Access-Control-Allow-Headers", "content-type", false);
    response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
    response.write("Access-Control-Allow-Origin: *");
    return;
  }

  const category = params.get("corsErrorCategory");
  switch (category) {
    case "CORSDidNotSucceed":
      corsDidNotSucceed(request, response);
      break;
    case "CORSExternalRedirectNotAllowed":
      corsExternalRedirectNotAllowed(request, response);
      break;
    case "CORSMissingAllowOrigin":
      corsMissingAllowOrigin(request, response);
      break;
    case "CORSMultipleAllowOriginNotAllowed":
      corsMultipleOriginNotAllowed(request, response);
      break;
    case "CORSAllowOriginNotMatchingOrigin":
      corsAllowOriginNotMatchingOrigin(request, response);
      break;
    case "CORSNotSupportingCredentials":
      corsNotSupportingCredentials(request, response);
      break;
    case "CORSMethodNotFound":
      corsMethodNotFound(request, response);
      break;
    case "CORSMissingAllowCredentials":
      corsMissingAllowCredentials(request, response);
      break;
    case "CORSPreflightDidNotSucceed":
      corsPreflightDidNotSucceed(request, response);
      break;
    case "CORSInvalidAllowMethod":
      corsInvalidAllowMethod(request, response);
      break;
    case "CORSInvalidAllowHeader":
      corsInvalidAllowHeader(request, response);
      break;
    case "CORSMissingAllowHeaderFromPreflight":
      corsMissingAllowHeaderFromPreflight(request, response);
      break;
  }
}

function corsDidNotSucceed(request, response) {
  setCacheHeaders(response);
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "http://example.com");
}

function corsExternalRedirectNotAllowed(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Headers", "content-type", false);
  response.setHeader("Location", "http://redirect.test/");
}

function corsMissingAllowOrigin(request, response) {
  setCacheHeaders(response);
  response.setStatusLine(request.httpVersion, 200, "corsMissingAllowOrigin");
}

function corsMultipleOriginNotAllowed(request, response) {
  // We can't set the same header twice with response.setHeader, so we need to seizePower
  // and write the response manually.
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Access-Control-Allow-Origin: *\r\n");
  response.write("Access-Control-Allow-Origin: mochi.test\r\n");
  response.write("\r\n");
  response.finish();
  setCacheHeaders(response);
}

function corsAllowOriginNotMatchingOrigin(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsAllowOriginNotMatchingOrigin");
  response.setHeader("Access-Control-Allow-Origin", "mochi.test");
}

function corsNotSupportingCredentials(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsNotSupportingCredentials");
  response.setHeader("Access-Control-Allow-Origin", "*");
}

function corsMethodNotFound(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsMethodNotFound");
  response.setHeader("Access-Control-Allow-Origin", "*");
  // Will make the request fail since it is a "PUT".
  response.setHeader("Access-Control-Allow-Methods", "POST");
}

function corsMissingAllowCredentials(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsMissingAllowCredentials");
  // Need to set an explicit origin (i.e. not "*") to make the request fail.
  response.setHeader("Access-Control-Allow-Origin", "http://example.com");
}

function corsPreflightDidNotSucceed(request, response) {
  const isPreflight = request.method == "OPTIONS";
  if (isPreflight) {
    response.setStatusLine(request.httpVersion, 500, "Preflight fail");
    response.setHeader("Access-Control-Allow-Origin", "*");
  }
}

function corsInvalidAllowMethod(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsInvalidAllowMethod");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Access-Control-Allow-Methods", "xyz;");
}

function corsInvalidAllowHeader(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsInvalidAllowHeader");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Access-Control-Allow-Methods", "PUT");
  response.setHeader("Access-Control-Allow-Headers", "xyz;");
}

function corsMissingAllowHeaderFromPreflight(request, response) {
  response.setStatusLine(request.httpVersion, 200, "corsMissingAllowHeaderFromPreflight");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Access-Control-Allow-Methods", "PUT");
}

function setCacheHeaders(response) {
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");
}
