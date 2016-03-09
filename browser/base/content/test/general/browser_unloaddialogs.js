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

add_task(function*() {
  for (let url of testUrls) {
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    ok(true, "Loaded page " + url);
    // Wait one turn of the event loop before closing, so everything settles.
    yield new Promise(resolve => setTimeout(resolve, 0));
    yield BrowserTestUtils.removeTab(tab);
    ok(true, "Closed page " + url + " without timeout");
  }
});
