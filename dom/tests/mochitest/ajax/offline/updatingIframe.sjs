ver1iframe =
  "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n" +
  "<head>\n" +
  "<title>Update iframe</title>\n" +
  "</head>\n" +
  "<body onload=\"parent.frameLoad(1)\">\n" +
  "First version of updating iframe.\n" +
  "</body>\n" +
  "</html>\n";

ver2iframe =
  "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n" +
  "<head>\n" +
  "<title>Update iframe</title>\n" +
  "</head>\n" +
  "<body onload=\"parent.frameLoad(2)\">\n" +
  "Second version of updating iframe.\n" +
  "</body>\n" +
  "</html>\n";

function handleRequest(request, response)
{
  var match = request.queryString.match(/^state=(.*)$/);
  if (match)
  {
    response.setStatusLine(request.httpVersion, 204, "No content");
    setState("offline.updatingIframe", match[1]);
  }

  if (request.queryString == "")
  {
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/html");
    response.setHeader("Cache-Control", "no-cache");
    switch (getState("offline.updatingIframe"))
    {
      case "": // The default value
        response.write(ver1iframe);
        break;
      case "second":
        response.write(ver2iframe);
        break;
    }
  }
}
