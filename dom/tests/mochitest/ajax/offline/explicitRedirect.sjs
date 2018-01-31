function handleRequest(request, response)
{
  var match = request.queryString.match(/^state=(.*)$/);
  if (match)
  {
    response.setStatusLine(request.httpVersion, 200, "No content");
    setState("state", match[1]);
    response.write("state='" + match[1] + "'");
  }

  if (request.queryString == "")
  {
    switch (getState("state"))
    {
      case "": // The default value
        response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
        response.setHeader("Location", "http://example.com/non-existing-explicit.html");
        response.setHeader("Content-Type", "text/html");
        break;
      case "on":
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "text/html");
        response.write("<html><body>Explicit page</body></html>");
        break;
    }
  }
}
