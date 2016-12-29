/* Make sure that the context menu appears on form elements */

add_task(function *() {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html,test");

  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");

  let tests = [
    { element: "input", type: "text" },
    { element: "input", type: "password" },
    { element: "input", type: "image" },
    { element: "input", type: "button" },
    { element: "input", type: "submit" },
    { element: "input", type: "reset" },
    { element: "input", type: "checkbox" },
    { element: "input", type: "radio" },
    { element: "button" },
    { element: "select" },
    { element: "option" },
    { element: "optgroup" }
  ];

  for (let index = 0; index < tests.length; index++) {
    let test = tests[index];

    yield ContentTask.spawn(gBrowser.selectedBrowser,
                            { element: test.element, type: test.type, index: index },
                            function* (arg) {
      let element = content.document.createElement(arg.element);
      element.id = "element" + arg.index;
      if (arg.type) {
        element.setAttribute("type", arg.type);
      }
      content.document.body.appendChild(element);
    });

    let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
    yield BrowserTestUtils.synthesizeMouseAtCenter("#element" + index,
          { type: "contextmenu", button: 2}, gBrowser.selectedBrowser);
    yield popupShownPromise;

    let typeAttr = test.type ? "type=" + test.type + " " : "";
    is(gContextMenu.shouldDisplay, true,
        "context menu behavior for <" + test.element + " " + typeAttr + "> is wrong");

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
    contentAreaContextMenu.hidePopup();
    yield popupHiddenPromise;
  }

  gBrowser.removeCurrentTab();
});
