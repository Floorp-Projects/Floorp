/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Content-Type", "application/json");
  let content = {
    accounts_endpoint:
      "https://example.net/tests/dom/credentialmanagement/identity/tests/mochitest/server_simple_accounts.sjs",
    client_metadata_endpoint:
      "https://example.net/tests/dom/credentialmanagement/identity/tests/mochitest/server_simple_metadata.sjs",
    id_token_endpoint:
      "https://example.net/tests/dom/credentialmanagement/identity/tests/mochitest/server_simple_idtoken.sjs",
  };
  let body = JSON.stringify(content);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(body);
}
