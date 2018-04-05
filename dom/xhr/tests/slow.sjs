function handleRequest(request, response)
{
  response.processAsync();

  timer = Components.classes["@mozilla.org/timer;1"].
          createInstance(Components.interfaces.nsITimer);
  timer.init(function() {
    response.write("Here the content. But slowly.");
    response.finish();
  }, 5000, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
