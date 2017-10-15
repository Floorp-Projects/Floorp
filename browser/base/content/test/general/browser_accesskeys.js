/* eslint-env mozilla/frame-script */

add_task(async function() {
  await pushPrefs(["ui.key.contentAccess", 5], ["ui.key.chromeAccess", 5]);

  const gPageURL1 = "data:text/html,<body><p>" +
                    "<button id='button' accesskey='y'>Button</button>" +
                    "<input id='checkbox' type='checkbox' accesskey='z'>Checkbox" +
                    "</p></body>";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL1);
  tab1.linkedBrowser.messageManager.loadFrameScript("data:,(" + childHandleFocus.toString() + ")();", false);

  Services.focus.clearFocus(window);

  // Press an accesskey in the child document while the chrome is focused.
  let focusedId = await performAccessKey("y");
  is(focusedId, "button", "button accesskey");

  // Press an accesskey in the child document while the content document is focused.
  focusedId = await performAccessKey("z");
  is(focusedId, "checkbox", "checkbox accesskey");

  // Add an element with an accesskey to the chrome and press its accesskey while the chrome is focused.
  let newButton = document.createElement("button");
  newButton.id = "chromebutton";
  newButton.setAttribute("accesskey", "z");
  document.documentElement.appendChild(newButton);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKeyForChrome("z");
  is(focusedId, "chromebutton", "chromebutton accesskey");

  // Add a second tab and ensure that accesskey from the first tab is not used.
  const gPageURL2 = "data:text/html,<body>" +
                    "<button id='tab2button' accesskey='y'>Button in Tab 2</button>" +
                    "</body>";
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL2);
  tab2.linkedBrowser.messageManager.loadFrameScript("data:,(" + childHandleFocus.toString() + ")();", false);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKey("y");
  is(focusedId, "tab2button", "button accesskey in tab2");

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = await performAccessKeyForChrome("z");
  is(focusedId, "chromebutton", "chromebutton accesskey");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);

  // Test whether access key for the newButton isn't available when content
  // consumes the key event.

  // When content in the tab3 consumes all keydown events.
  const gPageURL3 = "data:text/html,<body id='tab3body'>" +
                    "<button id='tab3button' accesskey='y'>Button in Tab 3</button>" +
                    "<script>" +
                    "document.body.addEventListener('keydown', (event)=>{ event.preventDefault(); });" +
                    "</script></body>";
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL3);
  tab3.linkedBrowser.messageManager.loadFrameScript("data:,(" + childHandleFocus.toString() + ")();", false);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKey("y");
  is(focusedId, "tab3button", "button accesskey in tab3 should be focused");

  newButton.onfocus = () => {
    ok(false, "chromebutton shouldn't get focus during testing with tab3");
  };

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = await performAccessKey("z");
  is(focusedId, "tab3body", "button accesskey in tab3 should keep having focus");

  newButton.onfocus = null;

  gBrowser.removeTab(tab3);

  // When content in the tab4 consumes all keypress events.
  const gPageURL4 = "data:text/html,<body id='tab4body'>" +
                    "<button id='tab4button' accesskey='y'>Button in Tab 4</button>" +
                    "<script>" +
                    "document.body.addEventListener('keypress', (event)=>{ event.preventDefault(); });" +
                    "</script></body>";
  let tab4 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL4);
  tab4.linkedBrowser.messageManager.loadFrameScript("data:,(" + childHandleFocus.toString() + ")();", false);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKey("y");
  is(focusedId, "tab4button", "button accesskey in tab4 should be focused");

  newButton.onfocus = () => {
    // EventStateManager handles accesskey before dispatching keypress event
    // into the DOM tree, therefore, chrome accesskey always wins focus from
    // content. However, this is different from shortcut keys.
    todo(false, "chromebutton shouldn't get focus during testing with tab4");
  };

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = await performAccessKey("z");
  is(focusedId, "tab4body", "button accesskey in tab4 should keep having focus");

  newButton.onfocus = null;

  gBrowser.removeTab(tab4);

  newButton.remove();
});

function childHandleFocus() {
  var sent = false;
  content.document.body.firstChild.addEventListener("focus", function focused(event) {
    sent = true;
    let focusedElement = content.document.activeElement;
    focusedElement.blur();
    sendAsyncMessage("Test:FocusFromAccessKey", { focus: focusedElement.id });
  }, true);
  content.document.body.addEventListener("keydown", function keydown(event) {
    sent = false;
  }, true);
  content.document.body.addEventListener("keyup", function keyup(event) {
    if (!sent) {
      sent = true;
      let focusedElement = content.document.activeElement;
      sendAsyncMessage("Test:FocusFromAccessKey", { focus: focusedElement.id });
    }
  });
}

function performAccessKey(key) {
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
async function performAccessKeyForChrome(key, inChild) {
  let waitFocusChangePromise = BrowserTestUtils.waitForEvent(document, "focus", true);
  EventUtils.synthesizeKey(key, { altKey: true, shiftKey: true });
  await waitFocusChangePromise;
  return document.activeElement.id;
}
