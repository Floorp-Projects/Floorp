function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var isPreflight = request.method == "OPTIONS";

  // Send response

  response.setHeader("Access-Control-Allow-Origin", query.allowOrigin);

  if (isPreflight) {
    var secData = {};

    if (request.hasHeader("Access-Control-Request-Headers")) {
      var magicHeader =
        request.getHeader("Access-Control-Request-Headers").split(",").
        filter(function(name) /^magic-/.test(name))[0];
    }

    if (magicHeader) {
      secData = eval(unescape(magicHeader.substr(6)));
      secData.allowHeaders = (secData.allowHeaders || "") + "," + magicHeader;
    }

    if (secData.allowHeaders)
      response.setHeader("Access-Control-Allow-Headers", secData.allowHeaders);

    if (secData.allowMethods)
      response.setHeader("Access-Control-Allow-Methods", secData.allowMethods);

    if (secData.cacheTime)
      response.setHeader("Access-Control-Max-Age", secData.cacheTime.toString());

    return;
  }

  response.setHeader("Content-Type", "application/xml", false);
  response.write("<res>hello pass</res>\n");
}
