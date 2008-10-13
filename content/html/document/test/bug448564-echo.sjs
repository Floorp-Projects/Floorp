function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  
  response.write("<script>");
  response.write("  parent.checkQueryString('" + request.queryString + "');");
  response.write("</script>");
}
