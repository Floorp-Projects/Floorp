
// Serves a file with a given mime type and size.

function getQuery(request) {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  return query;
}

function handleRequest(request, response) {
  var query = getQuery(request);

  // Default values for content type and size.
  var contentType = "text/plain";
  var size = 1024;

  if ("contentType" in query) {
    contentType = query["contentType"];
  }

  if ("size" in query) {
    size = query["size"];
  }

  // Generate the content.
  response.setHeader("Content-Type", contentType, false);

  for (let i = 0; i < size; i++) {
    response.write("0");
  }
}
