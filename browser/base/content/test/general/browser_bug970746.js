/* Make sure context menu includes option to search hyperlink text on search engine */

add_task(function *() {
  const url = "http://mochi.test:8888/browser/browser/base/content/test/general/browser_bug970746.xhtml";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const ellipsis = "\u2026";

  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");

  // Tests if the "Search <engine> for '<some terms>'" context menu item is shown for the
  // given query string of an element. Tests to make sure label includes the proper search terms.
  //
  // Each test:
  //
  //   id: The id of the element to test.
  //   isSelected: Flag to enable selecting (text highlight) the contents of the element
  //   shouldBeShown: The display state of the menu item
  //   expectedLabelContents: The menu item label should contain a portion of this string.
  //                          Will only be tested if shouldBeShown is true.
  let tests = [
    {
      id: "link",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm a link!",
    },
    {
      id: "link",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "I'm a link!",
    },
    {
      id: "longLink",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm a really lo" + ellipsis,
    },
    {
      id: "longLink",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "I'm a really lo" + ellipsis,
    },
    {
      id: "plainText",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "Right clicking " + ellipsis,
    },
    {
      id: "plainText",
      isSelected: false,
      shouldBeShown: false,
    },
    {
      id: "mixedContent",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm some text, " + ellipsis,
    },
    {
      id: "mixedContent",
      isSelected: false,
      shouldBeShown: false,
    },
    {
      id: "partialLink",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "link selection",
    },
    {
      id: "partialLink",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "A partial link " + ellipsis,
    },
    {
      id: "surrogatePair",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "This character\uD83D\uDD25" + ellipsis,
    }
  ];

  for (let test of tests) {
    yield ContentTask.spawn(gBrowser.selectedBrowser,
                            { selectElement: test.isSelected ? test.id : null },
                            function* (arg) {
      let selection = content.getSelection();
      selection.removeAllRanges();

      if (arg.selectElement) {
        selection.selectAllChildren(content.document.getElementById(arg.selectElement));
      }
    });

    let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
    yield BrowserTestUtils.synthesizeMouseAtCenter("#" + test.id,
          { type: "contextmenu", button: 2}, gBrowser.selectedBrowser);
    yield popupShownPromise;

    let menuItem = document.getElementById("context-searchselect");
    is(menuItem.hidden, !test.shouldBeShown,
        "search context menu item is shown for  '#" + test.id + "' and selected is '" + test.isSelected + "'");

    if (test.shouldBeShown) {
      ok(menuItem.label.includes(test.expectedLabelContents),
         "Menu item text '" + menuItem.label + "' contains the correct search terms '" + test.expectedLabelContents + "'");
    }

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
    contentAreaContextMenu.hidePopup();
    yield popupHiddenPromise;
  }

  // cleanup
  gBrowser.removeCurrentTab();
});
