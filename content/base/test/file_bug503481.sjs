function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  dump("@@@@@" + request.queryString);

  if (query.unblock) {
    let blockedResponse = null;
    try {
      getObjectState("bug503481_" + query.unblock, function(x) {blockedResponse = x.wrappedJSObject.r});
    } catch(e) {
      throw "unable to unblock '" + query.unblock + "': " + e.message;
    }
    setObjectState("bug503481_" + query.unblock, null);
    blockedResponse.finish();
  }

  if (query.blockOn) {
    response.processAsync();
    x = { r: response, QueryInterface: function(iid) { return this } };
    x.wrappedJSObject = x;
    setObjectState("bug503481_" + query.blockOn, x);
  }

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.write(query.body);
}
