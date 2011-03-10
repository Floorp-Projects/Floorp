// timer has to be alive so it can't be eaten by the GC.
var timer;

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  // The "stray" open comment at the end of the write is important!
  response.write("document.write(\"<script charset='utf-8' src='script-2_bug597345.js'></script><!--\")");
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.finish();
    }, 200, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
