function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/css", false);

  var accessCount;
  var state = getState("varSSAccessCount");
  if (!state) {
    setState("varSSAccessCount", "0");
    accessCount = 0;
  } else {
    setState("varSSAccessCount", (parseInt(state) + 1).toString());
    accessCount = parseInt(state) + 1;
  }

  response.write("#content { background-color: rgb(" +
                 (accessCount % 256) +
                 ", 0, 0); }");
}
