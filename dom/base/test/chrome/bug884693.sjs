function handleRequest(request, response)
{
  let [status, statusText, body] = request.queryString.split("&");
  response.setStatusLine(request.httpVersion, status, statusText);
  response.setHeader("Content-Type", "text/xml", false);
  response.setHeader("Content-Length", "" + body.length, false);
  response.write(body);
}
