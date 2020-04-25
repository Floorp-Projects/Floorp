function handleRequest(request, response) {
  var reconnecting = getState("eventsource_reconnecting");
  var data = reconnecting ? +reconnecting : 0;
  var body = "";
  if (!reconnecting) {
    body = "retry: 2\n";
    response.setStatusLine(request.httpVersion, 200, "OK");
    setState("eventsource_reconnecting", "0");
  } else if (reconnecting === "0") {
    response.setStatusLine(request.httpVersion, 204, "No Content");
    setState("eventsource_reconnecting", "");
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    setState("eventsource_reconnecting", "");
  }

  response.setHeader("Content-Type", "text/event-stream");
  response.setHeader("Cache-Control", "no-cache");

  body += "data: " + data + "\n\n";
  response.write(body);
}
