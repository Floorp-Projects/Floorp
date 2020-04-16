function handleRequest(request, response)
{
  var auth = "";
  try {
    auth = request.getHeader("Authorization");
  } catch(e) {}

  if (auth == "Basic aHR0cHVzZXI6aHR0cHBhc3M=") {
    response.setStatusLine("1.1", 200, "OK");
    response.write("<html><head><title>http auth success</title></head><html>");
  } else {
    response.setStatusLine("1.1", 401, "Http authentication required");
    response.setHeader("WWW-Authenticate", "Basic realm=\"http_realm\"");
    response.write("<html><head><title>http auth failed</title></head><html>");
  }
}