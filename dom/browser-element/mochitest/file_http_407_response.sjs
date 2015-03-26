function handleRequest(request, response)
{
  var auth = "";
  try {
    auth = request.getHeader("Proxy-Authorization");
  } catch(e) {}

  if (auth == "Basic cHJveHl1c2VyOnByb3h5cGFzcw==") {
    response.setStatusLine("1.1", 200, "OK");
    response.write("<html><head><title>http auth success</title></head><html>");
  } else {
    response.setStatusLine("1.1", 407, "Proxy Authentication Required");
    response.setHeader("Proxy-Authenticate", "Basic realm=\"http_realm\"");
    response.write("<html><head><title>http auth failed</title></head><html>");
  }
}
