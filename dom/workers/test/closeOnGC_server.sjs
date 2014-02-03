/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.method == "POST") {
    setState("seenPost" + request.queryString, "1");
    return;
  }

  if (request.method == "GET") {
    if (getState("seenPost" + request.queryString) == "1") {
      response.write("closed");
    }
    return;
  }

  response.setStatusLine(request.httpVersion, 404, "Not found");
}
