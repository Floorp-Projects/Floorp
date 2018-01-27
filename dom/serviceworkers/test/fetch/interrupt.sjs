function handleRequest(request, response) {
  var body = "a";
  for (var i = 0; i < 20; i++) {
    body += body;
  }

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n")
  var count = 10;
  response.write("Content-Length: " + body.length * count + "\r\n");
  response.write("Content-Type: text/plain; charset=utf-8\r\n");
  response.write("Cache-Control: no-cache, must-revalidate\r\n");
  response.write("\r\n");

  for (var i = 0; i < count; i++) {
    response.write(body);
  }

  throw Components.results.NS_BINDING_ABORTED;
}
