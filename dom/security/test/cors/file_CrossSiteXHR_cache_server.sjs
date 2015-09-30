function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  if ("setState" in query) {
    setState("test/dom/security/test_CrossSiteXHR_cache:secData",
             query.setState);

    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write("hi");

    return;
  }

  var isPreflight = request.method == "OPTIONS";

  // Send response

  secData =
    eval(getState("test/dom/security/test_CrossSiteXHR_cache:secData"));

  if (secData.allowOrigin)
    response.setHeader("Access-Control-Allow-Origin", secData.allowOrigin);

  if (secData.withCred)
    response.setHeader("Access-Control-Allow-Credentials", "true");

  if (isPreflight) {
    if (secData.allowHeaders)
      response.setHeader("Access-Control-Allow-Headers", secData.allowHeaders);

    if (secData.allowMethods)
      response.setHeader("Access-Control-Allow-Methods", secData.allowMethods);

    if (secData.cacheTime)
      response.setHeader("Access-Control-Max-Age", secData.cacheTime.toString());

    return;
  }

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/xml", false);
  response.write("<res>hello pass</res>\n");
}
