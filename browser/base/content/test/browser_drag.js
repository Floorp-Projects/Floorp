function test()
{
  waitForExplicitFinish();

  // ---- Test dragging the proxy icon ---
  var value = content.location.href;
  var urlString = value + "\n" + content.document.title;
  var htmlString = "<a href=\"" + value + "\">" + value + "</a>";
  var expected = [ [
    { type  : "text/x-moz-url",
      data  : urlString },
    { type  : "text/uri-list",
      data  : value },
    { type  : "text/plain",
      data  : value },
    { type  : "text/html",
      data  : htmlString }
  ] ];
  // set the valid attribute so dropping is allowed
  var oldstate = gURLBar.getAttribute("pageproxystate");
  gURLBar.setAttribute("pageproxystate", "valid");
  var dt = EventUtils.synthesizeDragStart(document.getElementById("identity-box"), expected);
  is(dt, null, "drag on proxy icon");
  gURLBar.setAttribute("pageproxystate", oldstate);
  // Now, the identity information panel is opened by the proxy icon click.
  // We need to close it for next tests.
  EventUtils.synthesizeKey("VK_ESCAPE", {}, window);

  // now test dragging onto a tab
  var tab = gBrowser.addTab("about:blank", {skipAnimation: true});
  var browser = gBrowser.getBrowserForTab(tab);

  browser.addEventListener("load", function () {
    is(browser.contentWindow.location, "http://mochi.test:8888/", "drop on tab");
    gBrowser.removeTab(tab);
    finish();
  }, true);

  EventUtils.synthesizeDrop(tab, tab, [[{type: "text/uri-list", data: "http://mochi.test:8888/"}]], "copy", window);
}
