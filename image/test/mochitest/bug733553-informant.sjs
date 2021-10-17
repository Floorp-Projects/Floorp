/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  // Tells bug733553.sjs that the consumer is ready for the next part
  let partName = request.queryString;
  setSharedState("next-part", partName);
  response.write("OK!");
}

