/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  response.setHeader("Access-Control-Allow-Origin", "https://example.com");
  response.setHeader("Access-Control-Allow-Credentials", "true");
  response.setHeader("Content-Type", "application/json");
  let responseContent = {
    token: "should not be returned",
  };
  let body = JSON.stringify(responseContent);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(body);
}
