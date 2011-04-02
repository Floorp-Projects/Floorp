function handleRequest(request, response) {
  var body = "initial";

  try {
    body = request.getHeader("X-Request");
  } catch(e) {
    body = "request.getHeader() failed! Exception: " + e;
  }

  response.setHeader("Cache-Control", "max-age=3600");
  response.bodyOutputStream.write(body, body.length);
}
