function handleRequest(request, response) {

  var query = {};

  request.queryString.split('&').forEach(function(val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  response.setHeader("Cache-Control", "no-cache", false);

  if ("type" in query) {
    switch (query.type) {
      case "script":
        response.setHeader("Content-Type", "application/javascript");
        response.write("\n\ndocument.write('<pre>script loaded\\n</pre>');\n\n");
        return;
      case "style":
        response.setHeader("Content-Type", "text/css");
        response.write("\n\n.cspfoo { color:red; }\n\n");
        return;
      case "img":
        response.setHeader("Content-Type", "image/png");
        return;
    }
  }

  response.setHeader("Content-Type", "text/plain");
  response.write("ohnoes!");
}
