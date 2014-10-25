function parseQuery(request, key) {
  var params = request.queryString.split('&');
  for (var j = 0; j < params.length; ++j) {
    var p = params[j];
	if (p == key)
	  return true;
    if (p.indexOf(key + "=") == 0)
	  return p.substring(key.length + 1);
	if (p.indexOf("=") < 0 && key == "")
	  return p;
  }
  return false;
}

// Return file content for the first request with a given key.
// All subsequent requests return a redirect to a different-origin resource.
function handleRequest(request, response)
{
  var domain = parseQuery(request, "domain");
  var file = parseQuery(request, "file");
  var allowed = parseQuery(request, "allowed");

  response.setStatusLine(request.httpVersion, 303, "See Other");
  response.setHeader("Location", "http://" + domain + "/tests/dom/media/test/" + (allowed ? "allowed.sjs?" : "") + file);
  response.setHeader("Content-Type", "text/html");
}
