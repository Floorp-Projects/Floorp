function handleRequest(request, response) {
  var match = request.queryString.match(/^status=(.*)$/);
  var status = match ? +match[1] : 200;

  if (status != 200) {
    response.setStatusLine(request.httpVersion, status, "Err");
  }
  response.setHeader("Content-Type", "text/event-stream");
  response.setHeader("Cache-Control", "no-cache");
  response.write("data: msg 1\n");
  response.write("id: 1\n\n");
}
