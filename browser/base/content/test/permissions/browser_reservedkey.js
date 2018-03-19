add_task(async function test_reserved_shortcuts() {
  /* eslint-disable no-unsanitized/property */
  let keyset = `<keyset>
                  <key id='kt_reserved' modifiers='shift' key='O' reserved='true' count='0'
                       oncommand='this.setAttribute("count", Number(this.getAttribute("count")) + 1)'/>
                  <key id='kt_notreserved' modifiers='shift' key='P' reserved='false' count='0'
                       oncommand='this.setAttribute("count", Number(this.getAttribute("count")) + 1)'/>
                  <key id='kt_reserveddefault' modifiers='shift' key='Q' count='0'
                       oncommand='this.setAttribute("count", Number(this.getAttribute("count")) + 1)'/>
                </keyset>`;

  let container = document.createElement("box");
  container.unsafeSetInnerHTML(keyset);
  document.documentElement.appendChild(container);
  /* eslint-enable no-unsanitized/property */

  const pageUrl = "data:text/html,<body onload='document.body.firstChild.focus();'><div onkeydown='event.preventDefault();' tabindex=0>Test</div></body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  EventUtils.sendString("OPQ");

  is(document.getElementById("kt_reserved").getAttribute("count"), "1", "reserved='true' with preference off");
  is(document.getElementById("kt_notreserved").getAttribute("count"), "0", "reserved='false' with preference off");
  is(document.getElementById("kt_reserveddefault").getAttribute("count"), "0", "default reserved with preference off");

  // Now try with reserved shortcut key handling enabled.
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["permissions.default.shortcuts", 2]]}, resolve);
  });

  EventUtils.sendString("OPQ");

  is(document.getElementById("kt_reserved").getAttribute("count"), "2", "reserved='true' with preference on");
  is(document.getElementById("kt_notreserved").getAttribute("count"), "0", "reserved='false' with preference on");
  is(document.getElementById("kt_reserveddefault").getAttribute("count"), "1", "default reserved with preference on");

  document.documentElement.removeChild(container);

  BrowserTestUtils.removeTab(tab);
});

// This test checks that Alt+<key> and F10 cannot be blocked when the preference is set.
if (!navigator.platform.includes("Mac")) {
  add_task(async function test_accesskeys_menus() {
    await new Promise(resolve => {
      SpecialPowers.pushPrefEnv({"set": [["permissions.default.shortcuts", 2]]}, resolve);
    });

    const uri = "data:text/html,<body onkeydown='if (event.key == \"H\" || event.key == \"F10\") event.preventDefault();'>";
    let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);

    // Pressing Alt+H should open the Help menu.
    let helpPopup = document.getElementById("menu_HelpPopup");
    let popupShown = BrowserTestUtils.waitForEvent(helpPopup, "popupshown");
    EventUtils.synthesizeKey("KEY_Alt", {type: "keydown"});
    EventUtils.synthesizeKey("h", {altKey: true});
    EventUtils.synthesizeKey("KEY_Alt", {type: "keyup"});
    await popupShown;

    ok(true, "Help menu opened");

    let popupHidden = BrowserTestUtils.waitForEvent(helpPopup, "popuphidden");
    helpPopup.hidePopup();
    await popupHidden;

    // Pressing F10 should focus the menubar. On Linux, the file menu should open, but on Windows,
    // pressing Down will open the file menu.
    let menubar = document.getElementById("main-menubar");
    let menubarActive = BrowserTestUtils.waitForEvent(menubar, "DOMMenuBarActive");
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
