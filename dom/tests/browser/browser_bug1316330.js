const URL =
  "data:text/html,<script>" +
  "window.focus();" + 
  "var down = 0; var press = 0;" +
  "onkeydown = function(e) {" +
  "  var startTime = Date.now();" +
  "  document.body.setAttribute('data-down', ++down);" +
  "  if (e.keyCode == KeyboardEvent.DOM_VK_D) while (Date.now() - startTime < 500) {}" +
  "};" +
  "onkeypress = function(e) {" +
  "  var startTime = Date.now();" +
  "  document.body.setAttribute('data-press', ++press);" +
  "  if (e.charCode == 'p'.charCodeAt(0)) while (Date.now() - startTime < 500) {}" +
  "};" +
  "</script>";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let browser = tab.linkedBrowser;

  await EventUtils.synthesizeAndWaitKey("d", { code: "KeyD", repeat: 3 });

  await ContentTask.spawn(browser, null, async function() {
    is(content.document.body.getAttribute("data-down"), "2", "Correct number of events");
    is(content.document.body.getAttribute("data-press"), "2", "Correct number of events");
  });

  await EventUtils.synthesizeAndWaitKey("p", { code: "KeyP", repeat: 3 });

  await ContentTask.spawn(browser, null, async function() {
    is(content.document.body.getAttribute("data-down"), "4", "Correct number of events");
    is(content.document.body.getAttribute("data-press"), "4", "Correct number of events");
  });

  gBrowser.removeCurrentTab();
});
