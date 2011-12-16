/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response)
{
  var Etag = '"4c881ab-b03-435f0a0f9ef00"';
  var IfNoneMatch = request.hasHeader("If-None-Match")
                    ? request.getHeader("If-None-Match")
                    : "";

  var page = "<!DOCTYPE html><html><body><p>hello world!</p></body></html>";

  response.setHeader("Etag", Etag, false);

  if (IfNoneMatch == Etag) {
    response.setStatusLine(request.httpVersion, "304", "Not Modified");
  }
  else {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Content-Length", page.length + "", false);
    response.write(page);
  }
}
