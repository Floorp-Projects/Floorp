// timer needs to be global so it can't be eaten by GC.
var timer;

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  response.write("ok(asyncRan, 'Async script should have run first.'); firstRan = true;");
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
                    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.finish();
    }, 200, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
