var timer = null; // declare timer outside to prevent premature GC
function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "text/plain", false);

  for (var i = 0; i < 1000; ++i)
    response.write("Hello... ");

  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.write("world.\n");
      response.finish();
    }, 10 * 1000 /* 10 secs */, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
