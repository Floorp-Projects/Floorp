function handleRequest(request, response)
{
  if (request.queryString) {
    let blockedResponse = null;
    getObjectState("bug602838", function(x) { blockedResponse = x.wrappedJSObject.r });
    blockedResponse.finish();
    setObjectState("bug602838", null);
    return;
  }
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  response.write("ok(asyncRan, 'Async script should have run first.'); firstRan = true;");
  response.processAsync();

  x = { r: response, QueryInterface: function(iid) { return this } };
  x.wrappedJSObject = x;
  setObjectState("bug602838", x);
}
