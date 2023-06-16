add_task(async function test_reserved_shortcuts() {
  let keyset = document.createXULElement("keyset");
  let key1 = document.createXULElement("key");
  key1.setAttribute("id", "kt_reserved");
  key1.setAttribute("modifiers", "shift");
  key1.setAttribute("key", "O");
  key1.setAttribute("reserved", "true");
  key1.setAttribute("count", "0");
  key1.addEventListener("command", () => {
    let attribute = key1.getAttribute("count");
    key1.setAttribute("count", Number(attribute) + 1);
  });

  let key2 = document.createXULElement("key");
  key2.setAttribute("id", "kt_notreserved");
  key2.setAttribute("modifiers", "shift");
  key2.setAttribute("key", "P");
  key2.setAttribute("reserved", "false");
  key2.setAttribute("count", "0");
  key2.addEventListener("command", () => {
    let attribute = key2.getAttribute("count");
    key2.setAttribute("count", Number(attribute) + 1);
  });

  let key3 = document.createXULElement("key");
  key3.setAttribute("id", "kt_reserveddefault");
  key3.setAttribute("modifiers", "shift");
  key3.setAttribute("key", "Q");
  key3.setAttribute("count", "0");
  key3.addEventListener("command", () => {
    let attribute = key3.getAttribute("count");
    key3.setAttribute("count", Number(attribute) + 1);
  });

  keyset.appendChild(key1);
  keyset.appendChild(key2);
  keyset.appendChild(key3);
  let container = document.createXULElement("box");
  container.appendChild(keyset);
  document.documentElement.appendChild(container);

  const pageUrl =
    "data:text/html,<body onload='document.body.firstElementChild.focus();'><div onkeydown='event.preventDefault();' tabindex=0>Test</div></body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  EventUtils.sendString("OPQ");

  is(
    document.getElementById("kt_reserved").getAttribute("count"),
    "1",
    "reserved='true' with preference off"
  );
  is(
    document.getElementById("kt_notreserved").getAttribute("count"),
    "0",
    "reserved='false' with preference off"
  );
  is(
    document.getElementById("kt_reserveddefault").getAttribute("count"),
    "0",
    "default reserved with preference off"
  );

  // Now try with reserved shortcut key handling enabled.
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      { set: [["permissions.default.shortcuts", 2]] },
      resolve
    );
  });

  EventUtils.sendString("OPQ");

  is(
    document.getElementById("kt_reserved").getAttribute("count"),
    "2",
    "reserved='true' with preference on"
  );
  is(
    document.getElementById("kt_notreserved").getAttribute("count"),
    "0",
    "reserved='false' with preference on"
  );
  is(
    document.getElementById("kt_reserveddefault").getAttribute("count"),
    "1",
    "default reserved with preference on"
  );

  document.documentElement.removeChild(container);

  BrowserTestUtils.removeTab(tab);
});

// This test checks that Alt+<key> and F10 cannot be blocked when the preference is set.
if (!navigator.platform.includes("Mac")) {
  add_task(async function test_accesskeys_menus() {
    await new Promise(resolve => {
      SpecialPowers.pushPrefEnv(
        { set: [["permissions.default.shortcuts", 2]] },
        resolve
      );
    });

    const uri =
      'data:text/html,<body onkeydown=\'if (event.key == "H" || event.key == "F10") event.preventDefault();\'>';
    let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);

    // Pressing Alt+H should open the Help menu.
    let helpPopup = document.getElementById("menu_HelpPopup");
    let popupShown = BrowserTestUtils.waitForEvent(helpPopup, "popupshown");
    EventUtils.synthesizeKey("KEY_Alt", { type: "keydown" });
    EventUtils.synthesizeKey("h", { altKey: true });
    EventUtils.synthesizeKey("KEY_Alt", { type: "keyup" });
    await popupShown;

    ok(true, "Help menu opened");

    let popupHidden = BrowserTestUtils.waitForEvent(helpPopup, "popuphidden");
    helpPopup.hidePopup();
    await popupHidden;

    // Pressing F10 should focus the menubar. On Linux, the file menu should open, but on Windows,
    // pressing Down will open the file menu.
    let menubar = document.getElementById("main-menubar");
    let menubarActive = BrowserTestUtils.waitForEvent(
      menubar,
      "DOMMenuBarActive"
    );
    EventUtils.synthesizeKey("KEY_F10");
    await menubarActive;

    let filePopup = document.getElementById("menu_FilePopup");
    popupShown = BrowserTestUtils.waitForEvent(filePopup, "popupshown");
    if (navigator.platform.includes("Win")) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    await popupShown;

    ok(true, "File menu opened");

    popupHidden = BrowserTestUtils.waitForEvent(filePopup, "popuphidden");
    filePopup.hidePopup();
    await popupHidden;

    BrowserTestUtils.removeTab(tab1);
  });
}

// There is a <key> element for Backspace and delete with reserved="false",
// so make sure that it is not treated as a blocked shortcut key.
add_task(async function test_backspace_delete() {
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      { set: [["permissions.default.shortcuts", 2]] },
      resolve
    );
  });

  // The input field is autofocused. If this test fails, backspace can go back
  // in history so cancel the beforeunload event and adjust the field to make the test fail.
  const uri =
    'data:text/html,<body onbeforeunload=\'document.getElementById("field").value = "failed";\'>' +
    "<input id='field' value='something'></body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.document.getElementById("field").focus();

    // Add a promise that resolves when the backspace key gets received
    // so we can ensure the key gets received before checking the result.
    content.keysPromise = new Promise(resolve => {
      content.addEventListener("keyup", event => {
        if (event.code == "Backspace") {
          resolve(content.document.getElementById("field").value);
        }
      });
    });
  });

  // Move the caret so backspace will delete the first character.
  EventUtils.synthesizeKey("KEY_ArrowRight", {});
  EventUtils.synthesizeKey("KEY_Backspace", {});

  let fieldValue = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      return content.keysPromise;
    }
  );
  is(fieldValue, "omething", "backspace not prevented");

  // now do the same thing for the delete key:
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.document.getElementById("field").focus();

    // Add a promise that resolves when the backspace key gets received
    // so we can ensure the key gets received before checking the result.
    content.keysPromise = new Promise(resolve => {
      content.addEventListener("keyup", event => {
        if (event.code == "Delete") {
          resolve(content.document.getElementById("field").value);
        }
      });
    });
  });

  // Move the caret so backspace will delete the first character.
  EventUtils.synthesizeKey("KEY_Delete", {});

  fieldValue = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      return content.keysPromise;
    }
  );
  is(fieldValue, "mething", "delete not prevented");

  BrowserTestUtils.removeTab(tab);
});

// TODO: Make this to run on Windows too to have automated tests also there.
if (
  navigator.platform.includes("Mac") ||
  navigator.platform.includes("Linux")
) {
  add_task(
    async function test_reserved_shortcuts_conflict_with_user_settings() {
      await new Promise(resolve => {
        SpecialPowers.pushPrefEnv(
          { set: [["test.events.async.enabled", true]] },
          resolve
        );
      });

      const keyset = document.createXULElement("keyset");
      const key = document.createXULElement("key");
      key.setAttribute("id", "conflict_with_known_native_key_binding");
      if (navigator.platform.includes("Mac")) {
        // Select to end of the paragraph
        key.setAttribute("modifiers", "ctrl,shift");
        key.setAttribute("key", "E");
      } else {
        // Select All
        key.setAttribute("modifiers", "ctrl");
        key.setAttribute("key", "a");
      }
      key.setAttribute("reserved", "true");
      key.setAttribute("count", "0");
      key.addEventListener("command", () => {
        const attribute = key.getAttribute("count");
        key.setAttribute("count", Number(attribute) + 1);
      });

      keyset.appendChild(key);
      const container = document.createXULElement("box");
      container.appendChild(keyset);
      document.documentElement.appendChild(container);

      const pageUrl =
        "data:text/html,<body onload='document.body.firstChild.focus(); getSelection().collapse(document.body.firstChild, 0)'><div contenteditable>Test</div></body>";
      const tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        pageUrl
      );

      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [key.getAttribute("key")],
        async function (aExpectedKeyValue) {
          content.promiseTestResult = new Promise(resolve => {
            content.addEventListener("keyup", event => {
              if (event.key.toLowerCase() == aExpectedKeyValue.toLowerCase()) {
                resolve(content.getSelection().getRangeAt(0).toString());
              }
            });
          });
        }
      );

      EventUtils.synthesizeKey(key.getAttribute("key"), {
        ctrlKey: key.getAttribute("modifiers").includes("ctrl"),
        shiftKey: key.getAttribute("modifiers").includes("shift"),
      });

      const selectedText = await SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        async function () {
          return content.promiseTestResult;
        }
      );
      is(
        selectedText,
        "Test",
        "The shortcut key should select all text in the editor"
      );

      is(
        key.getAttribute("count"),
        "0",
        "The reserved shortcut key should be consumed by the focused editor instead"
      );

      document.documentElement.removeChild(container);

      BrowserTestUtils.removeTab(tab);
    }
  );
}
