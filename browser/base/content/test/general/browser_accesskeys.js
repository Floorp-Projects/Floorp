add_task(async function() {
  await pushPrefs(["ui.key.contentAccess", 5], ["ui.key.chromeAccess", 5]);

  const gPageURL1 =
    "data:text/html,<body><p>" +
    "<button id='button' accesskey='y'>Button</button>" +
    "<input id='checkbox' type='checkbox' accesskey='z'>Checkbox" +
    "</p></body>";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL1);

  Services.focus.clearFocus(window);

  // Press an accesskey in the child document while the chrome is focused.
  let focusedId = await performAccessKey(tab1.linkedBrowser, "y");
  is(focusedId, "button", "button accesskey");

  // Press an accesskey in the child document while the content document is focused.
  focusedId = await performAccessKey(tab1.linkedBrowser, "z");
  is(focusedId, "checkbox", "checkbox accesskey");

  // Add an element with an accesskey to the chrome and press its accesskey while the chrome is focused.
  let newButton = document.createXULElement("button");
  newButton.id = "chromebutton";
  newButton.setAttribute("accesskey", "z");
  document.documentElement.appendChild(newButton);

  Services.focus.clearFocus(window);

  newButton.getBoundingClientRect(); // Accesskey registration happens during frame construction.

  focusedId = await performAccessKeyForChrome("z");
  is(focusedId, "chromebutton", "chromebutton accesskey");

  // Add a second tab and ensure that accesskey from the first tab is not used.
  const gPageURL2 =
    "data:text/html,<body>" +
    "<button id='tab2button' accesskey='y'>Button in Tab 2</button>" +
    "</body>";
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL2);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKey(tab2.linkedBrowser, "y");
  is(focusedId, "tab2button", "button accesskey in tab2");

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = await performAccessKeyForChrome("z");
  is(focusedId, "chromebutton", "chromebutton accesskey");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);

  // Test whether access key for the newButton isn't available when content
  // consumes the key event.

  // When content in the tab3 consumes all keydown events.
  const gPageURL3 =
    "data:text/html,<body id='tab3body'>" +
    "<button id='tab3button' accesskey='y'>Button in Tab 3</button>" +
    "<script>" +
    "document.body.addEventListener('keydown', (event)=>{ event.preventDefault(); });" +
    "</script></body>";
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL3);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKey(tab3.linkedBrowser, "y");
  is(focusedId, "tab3button", "button accesskey in tab3 should be focused");

  newButton.onfocus = () => {
    ok(false, "chromebutton shouldn't get focus during testing with tab3");
  };

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = await performAccessKey(tab3.linkedBrowser, "z");
  is(
    focusedId,
    "tab3body",
    "button accesskey in tab3 should keep having focus"
  );

  newButton.onfocus = null;

  gBrowser.removeTab(tab3);

  // When content in the tab4 consumes all keypress events.
  const gPageURL4 =
    "data:text/html,<body id='tab4body'>" +
    "<button id='tab4button' accesskey='y'>Button in Tab 4</button>" +
    "<script>" +
    "document.body.addEventListener('keypress', (event)=>{ event.preventDefault(); });" +
    "</script></body>";
  let tab4 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gPageURL4);

  Services.focus.clearFocus(window);

  focusedId = await performAccessKey(tab4.linkedBrowser, "y");
  is(focusedId, "tab4button", "button accesskey in tab4 should be focused");

  newButton.onfocus = () => {
    // EventStateManager handles accesskey before dispatching keypress event
    // into the DOM tree, therefore, chrome accesskey always wins focus from
    // content. However, this is different from shortcut keys.
    todo(false, "chromebutton shouldn't get focus during testing with tab4");
  };

  // Press the accesskey for the chrome element while the content document is focused.
  focusedId = await performAccessKey(tab4.linkedBrowser, "z");
  is(
    focusedId,
    "tab4body",
    "button accesskey in tab4 should keep having focus"
  );

  newButton.onfocus = null;

  gBrowser.removeTab(tab4);

  newButton.remove();
});

function performAccessKey(browser, key) {
  return new Promise(resolve => {
    let removeFocus, removeKeyDown, removeKeyUp;
    function callback(eventName, result) {
      removeFocus();
      removeKeyUp();
      removeKeyDown();

      SpecialPowers.spawn(browser, [], () => {
        let oldFocusedElement = content._oldFocusedElement;
        delete content._oldFocusedElement;
        return oldFocusedElement.id;
      }).then(oldFocus => resolve(oldFocus));
    }

    removeFocus = BrowserTestUtils.addContentEventListener(
      browser,
      "focus",
      callback,
      { capture: true },
      event => {
        if (!HTMLElement.isInstance(event.target)) {
          return false; // ignore window and document focus events
        }

        event.target.ownerGlobal._sent = true;
        let focusedElement = event.target.ownerGlobal.document.activeElement;
        event.target.ownerGlobal._oldFocusedElement = focusedElement;
        focusedElement.blur();
        return true;
      }
    );

    removeKeyDown = BrowserTestUtils.addContentEventListener(
      browser,
      "keydown",
      () => {},
      { capture: true },
      event => {
        event.target.ownerGlobal._sent = false;
        return true;
      }
    );

    removeKeyUp = BrowserTestUtils.addContentEventListener(
      browser,
      "keyup",
      callback,
      {},
      event => {
        if (!event.target.ownerGlobal._sent) {
          event.target.ownerGlobal._sent = true;
          let focusedElement = event.target.ownerGlobal.document.activeElement;
          event.target.ownerGlobal._oldFocusedElement = focusedElement;
          focusedElement.blur();
          return true;
        }

        return false;
      }
    );

    // Spawn an no-op content task to better ensure that the messages
    // for adding the event listeners above get handled.
    SpecialPowers.spawn(browser, [], () => {}).then(() => {
      EventUtils.synthesizeKey(key, { altKey: true, shiftKey: true });
    });
  });
}

// This version is used when a chrome element is expected to be found for an accesskey.
async function performAccessKeyForChrome(key, inChild) {
  let waitFocusChangePromise = BrowserTestUtils.waitForEvent(
    document,
    "focus",
    true
  );
  EventUtils.synthesizeKey(key, { altKey: true, shiftKey: true });
  await waitFocusChangePromise;
  return document.activeElement.id;
}
