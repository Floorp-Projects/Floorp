/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response)
{
  var page = "<!DOCTYPE html><html><body><p>hello world! bug 630733</p></body></html>";

  response.setStatusLine(request.httpVersion, "301", "Moved Permanently");
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Length", page.length + "", false);
  response.setHeader("x-foobar-bug630733", "bazbaz", false);
  response.setHeader("Location", "/redirect-from-bug-630733", false);
  response.write(page);
}
