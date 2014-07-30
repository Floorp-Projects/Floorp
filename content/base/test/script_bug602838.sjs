function setOurState(data) {
  x = { data: data, QueryInterface: function(iid) { return this } };
  x.wrappedJSObject = x;
  setObjectState("bug602838", x);
}

function getOurState() {
  var data;
  getObjectState("bug602838", function(x) {
    // x can be null if no one has set any state yet
    if (x) {
      data = x.wrappedJSObject.data;
    }
  });
  return data;
}

function handleRequest(request, response)
{
  if (request.queryString) {
    let blockedResponse = getOurState();
    if (typeof(blockedResponse) == "object") {
      blockedResponse.finish();
      setOurState(null);
    } else {
      setOurState("unblocked");
    }
    return;
  }
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  response.write("ok(asyncRan, 'Async script should have run first.'); firstRan = true;");
  if (getOurState() != "unblocked") {
    response.processAsync();
    setOurState(response);
  }
}
