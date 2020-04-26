function handleRequest(request, response) {
  var name = "eventsource_reconnecting_" + request.queryString;
  var reconnecting = getState(name);
  var body = "";
  if (!reconnecting) {
    body = "retry: 2\n";
    setState(name, "0");
  } else if (reconnecting === "0") {
    setState(name, "");
    response.setStatusLine(request.httpVersion, 204, "No Content");
  }

  response.setHeader("Content-Type", "text/event-stream");
  response.setHeader("Cache-Control", "no-cache");

  body += "data: 1\n\n";
  response.write(body);
}
