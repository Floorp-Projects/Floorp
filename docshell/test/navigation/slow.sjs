function handleRequest(request, response)
{
  response.processAsync();

  timer = Components.classes["@mozilla.org/timer;1"].
          createInstance(Components.interfaces.nsITimer);
  timer.init(function() {
    response.finish();
  }, 5000, Components.interfaces.nsITimer.TYPE_ONE_SHOT);

  response.setStatusLine(null, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.write("Start of the content.");
}
