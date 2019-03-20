function handleRequest(request, response)
{
  let [status, statusText, encodedBody] = request.queryString.split("&");
  let body = decodeURIComponent(encodedBody);
  response.setStatusLine(request.httpVersion, status, statusText);
  response.setHeader("Content-Type", "text/xml", false);
  response.setHeader("Content-Length", "" + body.length, false);
  response.write(body);
}
