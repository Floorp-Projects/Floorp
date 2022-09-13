/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  response.setHeader("Access-Control-Allow-Origin", "https://example.com");
  response.setHeader("Access-Control-Allow-Credentials", "true");
  response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
}
