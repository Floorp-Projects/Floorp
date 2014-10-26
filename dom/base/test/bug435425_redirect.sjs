function handleRequest(request, response)
{
  response.setStatusLine(null, 302, "Moved");
  response.setHeader("Location", "http://www.mozilla.org", false);
}

