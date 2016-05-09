add_task(function* test() {
  const kTestURI =
    "data:text/html," +
    "<script type=\"text/javascript\">" +
    "  function onMouseDown(aEvent) {" +
    "    document.getElementById('willBeFocused').focus();" +
    "    aEvent.preventDefault();" +
    "  }" +
    "</script>" +
    "<body id=\"body\">" +
    "<button onmousedown=\"onMouseDown(event);\" style=\"width: 100px; height: 100px;\">click here</button>" +
    "<input id=\"willBeFocused\"></body>";

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kTestURI);

  let fm = Components.classes["@mozilla.org/focus-manager;1"].
        getService(Components.interfaces.nsIFocusManager);

  for (var button = 0; button < 3; button++) {
    // Set focus to a chrome element before synthesizing a mouse down event.
    document.getElementById("urlbar").focus();

    is(fm.focusedElement, document.getElementById("urlbar").inputField,
       "Failed to move focus to search bar: button=" + button);

    // Synthesize mouse down event on browser object over the button, such that
    // the event propagates through both processes.
    EventUtils.synthesizeMouse(tab.linkedBrowser, 20, 20, { "button": button }, null);

    isnot(fm.focusedElement, document.getElementById("urlbar").inputField,
       "Failed to move focus away from search bar: button=" + button);

    yield ContentTask.spawn(tab.linkedBrowser, button, function (button) {
      let fm = Components.classes["@mozilla.org/focus-manager;1"].
            getService(Components.interfaces.nsIFocusManager);

      Assert.equal(content.document.activeElement.id, "willBeFocused",
                   "The input element isn't active element: button=" + button);
      Assert.equal(fm.focusedElement, content.document.activeElement,
                   "The active element isn't focused element in App level: button=" + button);
    });
  }

  gBrowser.removeTab(tab);
});
