function handleRequest(request, response)
{
  response.setHeader("Set-Cookie", "cookie="+ request.host + "~" + Math.random());
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);

  response.setStatusLine(request.httpVersion, 200, "OK");
}
