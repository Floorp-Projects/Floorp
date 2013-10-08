var testUrls =
  [
    "data:text/html,<script>" +
      "function handle(evt) {" +
        "evt.target.removeEventListener(evt.type, handle, true);" +
        "try { alert('This should NOT appear'); } catch(e) { }" +
      "}" +
      "window.addEventListener('pagehide', handle, true);" +
      "window.addEventListener('beforeunload', handle, true);" +
      "window.addEventListener('unload', handle, true);" +
      "</script><body>Testing alert during pagehide/beforeunload/unload</body>",
    "data:text/html,<script>" +
      "function handle(evt) {" +
        "evt.target.removeEventListener(evt.type, handle, true);" +
        "try { prompt('This should NOT appear'); } catch(e) { }" +
      "}" +
      "window.addEventListener('pagehide', handle, true);" +
      "window.addEventListener('beforeunload', handle, true);" +
      "window.addEventListener('unload', handle, true);" +
      "</script><body>Testing prompt during pagehide/beforeunload/unload</body>",
    "data:text/html,<script>" +
      "function handle(evt) {" +
        "evt.target.removeEventListener(evt.type, handle, true);" +
        "try { confirm('This should NOT appear'); } catch(e) { }" +
      "}" +
      "window.addEventListener('pagehide', handle, true);" +
      "window.addEventListener('beforeunload', handle, true);" +
      "window.addEventListener('unload', handle, true);" +
      "</script><body>Testing confirm during pagehide/beforeunload/unload</body>",
  ];
var testsDone = 0;

function test()
{
  waitForExplicitFinish();
  runTest();
}

function runTest()
{
  whenNewTabLoaded(window, function() {
    gBrowser.selectedBrowser.addEventListener("load", onLoad, true);
    executeSoon(function() {
      info("Loading page with pagehide, beforeunload, and unload handlers that attempt to create dialogs");
      gBrowser.selectedBrowser.loadURI(testUrls[testsDone]);
    });
  });
}

function onLoad(event)
{
  info("Page loaded");

  event.target.removeEventListener("load", onLoad, true);
  gBrowser.selectedBrowser.addEventListener("unload", done, true);

  executeSoon(function () {
    info("Closing page");
    gBrowser.removeCurrentTab();
  });
}

function done() {
  ok(true, "Page closed (hopefully) without timeout");

  testsDone++;
  if (testsDone == testUrls.length) {
    finish();
    return;
  }

  executeSoon(runTest);
}
