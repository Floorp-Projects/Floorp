/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.importGlobalProperties(["URLSearchParams"]);

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  let test = params.get("set_test");
  if (test === null) {
    test = getState("test");
  } else {
    setState("test", test);
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setStatusLine(request.httpVersion, 200, "OK");
    return;
  }

  if (request.hasHeader("Cookie")) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    return;
  }
  if (request.hasHeader("Origin") && request.getHeader("Origin") != "null") {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    return;
  }
  if (request.hasHeader("Referer")) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    return;
  }

  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Content-Type", "application/json");
  let content = {
    accounts_endpoint:
      "https://example.net/tests/dom/credentialmanagement/identity/tests/mochitest/server_TESTNAME_accounts.sjs",
    client_metadata_endpoint:
      "https://example.net/tests/dom/credentialmanagement/identity/tests/mochitest/server_TESTNAME_metadata.sjs",
    id_token_endpoint:
      "https://example.net/tests/dom/credentialmanagement/identity/tests/mochitest/server_TESTNAME_idtoken.sjs",
  };
  let bodyFormat = JSON.stringify(content);
  let body = bodyFormat.replaceAll("TESTNAME", test);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(body);
}
