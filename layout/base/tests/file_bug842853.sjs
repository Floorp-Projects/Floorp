var timer;

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache, must-revalidate", false);
  response.setHeader("Content-Type", "text/css", false);
  response.write("body { background:lime; color:red; }");
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.finish();
    }, 500, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
