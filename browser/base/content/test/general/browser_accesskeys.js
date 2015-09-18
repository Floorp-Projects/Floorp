add_task(function *() {
  yield pushPrefs(["ui.key.contentAccess", 5], ["ui.key.chromeAccess", 5]);

  const gPageURL1 = "data:text/html,<body><p>" +
                    "<button id='button' accesskey='y'>Button</button>" +
                    "<input id='checkbox' type='checkbox' accesskey='z'>Checkbox" +
                    "</p></body>";
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL1);
  tab1.linkedBrowser.messageManager.loadFrameScript("data:,(" + childHandleFocus.toString() + ")();", false);

  Services.focus.clearFocus(window);

  // Press an accesskey in the child document while the chrome is focused.
  let focusedId = yield performAccessKey("y");
  is(focusedId, "button", "button accesskey");

  // Press an accesskey in the child document while the content document is focused.
  focusedId = yield performAccessKey("z");
  is(focusedId, "checkbox", "checkbox accesskey");

  // Add an element with an accesskey to the chrome and press its accesskey while the chrome is focused.
  let newButton = document.createElement("button");
  newButton.id = "chromebutton";
  newButton.setAttribute("accesskey", "z");
  document.documentElement.appendChild(newButton);

  Services.focus.clearFocus(window);

  focusedId = yield performAccessKeyForChrome("z");
  is(focusedId, "chromebutton", "chromebutton accesskey");

  // Add a second tab and ensure that accesskey from the first tab is not used.
  const gPageURL2 = "data:text/html,<body>" +
                    "<button id='tab2button' accesskey='y'>Button in Tab 2</button>" +
                    "</body>";
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL2);
  tab2.linkedBrowser.messageManager.loadFrameScript("data:,(" + childHandleFocus.toString() + ")();", false);

  Services.focus.clearFocus(window);

  focusedId = yield performAccessKey("y");
  is(focusedId, "tab2button", "button accesskey in tab2");

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = yield performAccessKeyForChrome("z");
  is(focusedId, "chromebutton", "chromebutton accesskey");

  newButton.parentNode.removeChild(newButton);

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});

function childHandleFocus() {
  content.document.body.firstChild.addEventListener("focus", function focused(event) {
    let focusedElement = content.document.activeElement;
    focusedElement.blur();
    sendAsyncMessage("Test:FocusFromAccessKey", { focus: focusedElement.id })
  }, true);
}

function performAccessKey(key)
{
  return new Promise(resolve => {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.addMessageListener("Test:FocusFromAccessKey", function listenForFocus(msg) {
      mm.removeMessageListener("Test:FocusFromAccessKey", listenForFocus);
      resolve(msg.data.focus);
    });

    EventUtils.synthesizeKey(key, { altKey: true, shiftKey: true });
  });
}

// This version is used when a chrome elemnt is expected to be found for an accesskey.
function* performAccessKeyForChrome(key, inChild)
{
  let waitFocusChangePromise = BrowserTestUtils.waitForEvent(document, "focus", true);
  EventUtils.synthesizeKey(key, { altKey: true, shiftKey: true });
  yield waitFocusChangePromise;
  return document.activeElement.id;
}
