/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200);
  response.setHeader("Set-Cookie", "foopy=1");
  response.write("cookie served");
}
