function test() {
  waitForExplicitFinish();

  const kTestURI =
    "data:text/html," +
    "<script type=\"text/javascript\">" +
    "  function onMouseDown(aEvent) {" +
    "    document.getElementById('willBeFocused').focus();" +
    "    aEvent.preventDefault();" +
    "  }" +
    "</script>" +
    "<body id=\"body\">" +
    "<button id=\"eventTarget\" onmousedown=\"onMouseDown(event);\">click here</button>" +
    "<input id=\"willBeFocused\"></body>";

  let tab = gBrowser.selectedTab;

  // Set the focus to the contents.
  tab.linkedBrowser.focus();
  // Load on the tab
  tab.linkedBrowser.addEventListener("load", onLoadTab, true);
  tab.linkedBrowser.loadURI(kTestURI);

  function onLoadTab() {
    tab.linkedBrowser.removeEventListener("load", onLoadTab, true);
    setTimeout(doTest, 0);
  }

  function doTest() {
    let fm = Components.classes["@mozilla.org/focus-manager;1"].
                        getService(Components.interfaces.nsIFocusManager);
    let eventTarget =
      tab.linkedBrowser.contentDocument.getElementById("eventTarget");

    for (var button = 0; button < 3; button++) {
      // Set focus to a chrome element before synthesizing a mouse down event.
      document.getElementById("urlbar").focus();

      is(fm.focusedElement, document.getElementById("urlbar").inputField,
         "Failed to move focus to search bar: button=" +
         button);

      EventUtils.synthesizeMouse(eventTarget, 10, 10, { "button": button },
                                 tab.linkedBrowser.contentWindow);

      let e = tab.linkedBrowser.contentDocument.activeElement;
      is(e.id, "willBeFocused",
         "The input element isn't active element: button=" + button);

      is(fm.focusedElement, e,
         "The active element isn't focused element in App level: button=" +
         button);
    }

    gBrowser.addTab();
    gBrowser.removeTab(tab);
    finish();
  }
}
