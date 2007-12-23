function test() {
  waitForExplicitFinish();

  // XXX This looks a bit odd, but is needed to avoid throwing when removing the
  // event listeners below. See bug 310955.
  document.getElementById("sidebar").addEventListener("load", function() { setTimeout(openPanelUrl, 100) }, true);
  toggleSidebar("viewWebPanelsSidebar", true);
}

function openPanelUrl(event) {
  ok(!document.getElementById("sidebar-box").hidden, "Sidebar showing");

  var sidebar = document.getElementById("sidebar");
  var root = sidebar.contentDocument.documentElement;
  ok(root.nodeName != "parsererror", "Sidebar is well formed");

  sidebar.removeEventListener("load", openPanelUrl, true);
  // XXX See comment above
  sidebar.contentDocument.addEventListener("load", function() { setTimeout(runTest, 100) }, true);
  var url = 'data:text/html,<div%20id="test_bug409481">Content!</div>';
  sidebar.contentWindow.loadWebPanel(url);
}

function runTest(event) {
  var sidebar = document.getElementById("sidebar");
  sidebar.contentDocument.removeEventListener("load", runTest, true);

  var browser = sidebar.contentDocument.getElementById("web-panels-browser");
  var div = browser && browser.contentDocument.getElementById("test_bug409481");
  ok(div && div.textContent == "Content!", "Sidebar content loaded");

  toggleSidebar("viewWebPanelsSidebar");

  ok(document.getElementById("sidebar-box").hidden, "Sidebar successfully hidden");

  finish();
}
