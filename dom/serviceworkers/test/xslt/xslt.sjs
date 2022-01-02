function handleRequest(request, response) {
  response.setHeader("Content-Type", "application/xslt+xml", false);
  response.setHeader("Access-Control-Allow-Origin", "*");

  var body = request.queryString;
  if (!body) {
    response.setStatusLine(null, 500, "Invalid querystring");
    return;
  }

  response.write(unescape(body));
}
