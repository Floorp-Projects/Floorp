// SJS file for CSP mochitests

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var isPreflight = request.method == "OPTIONS";


  //avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if ("type" in query) {
    response.setHeader("Content-Type", unescape(query['type']), false);
  } else {
    response.setHeader("Content-Type", "text/html", false);
  }

  if ("content" in query) {
    response.setHeader("Content-Type", "text/html", false);
    response.write(unescape(query['content']));
  }
}
