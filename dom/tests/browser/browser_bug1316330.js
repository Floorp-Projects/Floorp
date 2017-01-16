const URL =
  "data:text/html,<script>" +
  "window.focus();" + 
  "var down = 0; var press = 0;" +
  "onkeydown = function(e) {" +
  "  document.body.setAttribute('data-down', ++down);" +
  "  if (e.keyCode == 'D') while (Date.now() - startTime < 500) {}" +
  "};" +
  "onkeypress = function(e) {" +
  "  document.body.setAttribute('data-press', ++press);" +
  "  if (e.charCode == 'P') while (Date.now() - startTime < 500) {}" +
  "};" +
  "</script>";

function createKeyEvent(type, id, repeated) {
  var code = id.charCodeAt(0);
  return new KeyboardEvent(type, { keyCode: code, charCode: code, bubbles: true, repeat: repeated });
}

add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let browser = tab.linkedBrowser;

  // Need to dispatch a DOM Event explicitly via PresShell to get KeyEvent with .repeat = true to 
  // be handled by EventStateManager, which then forwards the event to the child process.
  var utils = EventUtils._getDOMWindowUtils(window);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keydown", "D", false), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keypress", "D", false), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keydown", "D", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keypress", "D", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keydown", "D", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keypress", "D", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keyup", "D", true), true);

  yield ContentTask.spawn(browser, null, function* () {
    is(content.document.body.getAttribute("data-down"), "2", "Correct number of events");
    is(content.document.body.getAttribute("data-press"), "2", "Correct number of events");
  });

  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keydown", "P", false), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keypress", "P", false), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keydown", "P", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keypress", "P", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keydown", "P", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keypress", "P", true), true);
  utils.dispatchDOMEventViaPresShell(browser, createKeyEvent("keyup", "P", true), true);

  yield ContentTask.spawn(browser, null, function* () {
    is(content.document.body.getAttribute("data-down"), "4", "Correct number of events");
    is(content.document.body.getAttribute("data-press"), "4", "Correct number of events");
  });

  gBrowser.removeCurrentTab();
});
