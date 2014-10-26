var timer = null;

function handleRequest(request, response)
{
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
                    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function()
  {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.write("hello");
    response.finish();
  }, 3000 /* milliseconds */, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
