// 'timer' is global to avoid getting GCed which would cancel the timer
var timer;
const nsITimer = Components.interfaces.nsITimer;

function attemptUnblock(s) {
  try {
    let blockedResponse = null;
    getObjectState("bug503481_" + s, function(x) {blockedResponse = x.wrappedJSObject.r});
    blockedResponse.finish();
    setObjectState("bug503481_" + s, null);
  } catch(e) {
    dump("unable to unblock " + s + "retrying in half a second\n");
    timer = Components.classes["@mozilla.org/timer;1"]
                      .createInstance(nsITimer);
    timer.initWithCallback(function () { attemptUnblock(s) }, 500, nsITimer.TYPE_ONE_SHOT);
  }
}

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  dump("processing:" + request.queryString + "\n");

  if (query.unblock) {
    attemptUnblock(query.unblock);
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
