function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "application/javascript", false);

  timer = Components.classes["@mozilla.org/timer;1"].
          createInstance(Components.interfaces.nsITimer);
  timer.init(function() {
    response.write("42");
    response.finish();
  }, 3000, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
