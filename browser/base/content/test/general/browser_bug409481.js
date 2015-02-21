function test() {
  waitForExplicitFinish();

  // XXX This looks a bit odd, but is needed to avoid throwing when removing the
  // event listeners below. See bug 310955.
  document.getElementById("sidebar").addEventListener("load", delayedOpenUrl, true);
  SidebarUI.show("viewWebPanelsSidebar");
}

function delayedOpenUrl() {
  ok(true, "Ran delayedOpenUrl");
  setTimeout(openPanelUrl, 100);
}

function openPanelUrl(event) {
  ok(!document.getElementById("sidebar-box").hidden, "Sidebar showing");

  var sidebar = document.getElementById("sidebar");
  var root = sidebar.contentDocument.documentElement;
  ok(root.nodeName != "parsererror", "Sidebar is well formed");

  sidebar.removeEventListener("load", delayedOpenUrl, true);
  // XXX See comment above
  sidebar.contentDocument.addEventListener("load", delayedRunTest, true);
  var url = 'data:text/html,<div%20id="test_bug409481">Content!</div><a id="link" href="http://www.example.com/ctest">Link</a><input id="textbox">';
  sidebar.contentWindow.loadWebPanel(url);
}

function delayedRunTest() {
  ok(true, "Ran delayedRunTest");
  setTimeout(runTest, 100);
}

function runTest(event) {
  var sidebar = document.getElementById("sidebar");
  sidebar.contentDocument.removeEventListener("load", delayedRunTest, true);

  var browser = sidebar.contentDocument.getElementById("web-panels-browser");
  var div = browser && browser.contentDocument.getElementById("test_bug409481");
  ok(div && div.textContent == "Content!", "Sidebar content loaded");

  var link = browser && browser.contentDocument.getElementById("link");
  sidebar.contentDocument.addEventListener("popupshown", contextMenuOpened, false);

  EventUtils.synthesizeMouseAtCenter(link, { type: "contextmenu", button: 2 }, browser.contentWindow);
}

function contextMenuOpened()
{
  var sidebar = document.getElementById("sidebar");
  sidebar.contentDocument.removeEventListener("popupshown", contextMenuOpened, false);

  var copyLinkCommand = sidebar.contentDocument.getElementById("context-copylink");
  copyLinkCommand.addEventListener("command", copyLinkCommandExecuted, false);
  copyLinkCommand.doCommand();
}

function copyLinkCommandExecuted(event)
{
  event.target.removeEventListener("command", copyLinkCommandExecuted, false);

  var sidebar = document.getElementById("sidebar");
  var browser = sidebar.contentDocument.getElementById("web-panels-browser");
  var textbox = browser && browser.contentDocument.getElementById("textbox");
  textbox.focus();
  document.commandDispatcher.getControllerForCommand("cmd_paste").doCommand("cmd_paste");
  is(textbox.value, "http://www.example.com/ctest", "copy link command");

  sidebar.contentDocument.addEventListener("popuphidden", contextMenuClosed, false);
  event.target.parentNode.hidePopup();
}

function contextMenuClosed()
{
  var sidebar = document.getElementById("sidebar");
  sidebar.contentDocument.removeEventListener("popuphidden", contextMenuClosed, false);

  SidebarUI.hide();

  ok(document.getElementById("sidebar-box").hidden, "Sidebar successfully hidden");

  finish();
}
