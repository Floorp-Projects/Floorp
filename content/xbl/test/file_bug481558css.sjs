function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  response.setHeader("Content-Type", "text/css", false);
  css = "#" + query.id + " { -moz-binding: url(\"";
  if (query.server) {
    css += "http://" + query.server + "/tests/content/xbl/test/";
  }
  css += "file_bug481558.xbl#test\"); }";

  response.write(css);
}
