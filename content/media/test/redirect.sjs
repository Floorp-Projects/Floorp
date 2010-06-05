// Return file content for the first request with a given key.
// All subsequent requests return a redirect to a different-origin resource.
function handleRequest(request, response)
{
  var params = request.queryString.split('&');
  var domain = null;
  var file = null;
  var allowed = false;

  for (var i=0; i<params.length; i++) {
    var kv = params[i].split('=');
    if (kv.length == 1 && kv[0] == 'allowed') {
      allowed = true;
    } else if (kv.length == 2 && kv[0] == 'file') {
      file = kv[1];
    } else if (kv.length == 2 && kv[0] == 'domain') {
      domain = kv[1];
    }
  }

  response.setStatusLine(request.httpVersion, 303, "See Other");
  response.setHeader("Location", "http://" + domain + "/tests/content/media/test/" + (allowed ? "allowed.sjs?" : "") + file);
  response.setHeader("Content-Type", "text/html");

}
