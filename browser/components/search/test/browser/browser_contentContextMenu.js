/* Make sure context menu includes option to search hyperlink text on search
 * engine.
 */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault", true],
      ["browser.search.separatePrivateDefault.ui.enabled", true],
    ],
  });

  const url =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/browser_contentContextMenu.xhtml";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const ellipsis = "\u2026";

  let contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );

  const originalPrivateDefault = await Services.search.getDefaultPrivate();
  let otherPrivateDefault;
  for (let engine of await Services.search.getVisibleEngines()) {
    if (engine.name != originalPrivateDefault.name) {
      otherPrivateDefault = engine;
      break;
    }
  }

  // Tests if the "Search <engine> for '<some terms>'" context menu item is
  // shown for the given query string of an element. Tests to make sure label
  // includes the proper search terms.
  //
  // Each test:
  //
  //   id: The id of the element to test.
  //   isSelected: Flag to enable selecting (text highlight) the contents of the
  //               element.
  //   shouldBeShown: The display state of the menu item.
  //   expectedLabelContents: The menu item label should contain a portion of
  //                          this string. Will only be tested if shouldBeShown
  //                          is true.
  //   shouldPrivateBeShown: The display state of the Private Window menu item.
  //   expectedPrivateLabelContents: The menu item label for the Private Window
  //                                 should contain a portion of this string.
  //                                 Will only be tested if shouldPrivateBeShown
  //                                 is true.
  let tests = [
    {
      id: "link",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm a link!",
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "link",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "I'm a link!",
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "longLink",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm a really lo" + ellipsis,
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "longLink",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "I'm a really lo" + ellipsis,
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "plainText",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "Right clicking " + ellipsis,
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "plainText",
      isSelected: false,
      shouldBeShown: false,
      shouldPrivateBeShown: false,
    },
    {
      id: "mixedContent",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm some text, " + ellipsis,
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "mixedContent",
      isSelected: false,
      shouldBeShown: false,
      shouldPrivateBeShown: false,
    },
    {
      id: "partialLink",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "link selection",
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search in",
    },
    {
      id: "partialLink",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "A partial link " + ellipsis,
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search with " + otherPrivateDefault.name,
      changePrivateDefaultEngine: true,
    },
    {
      id: "surrogatePair",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "This character\uD83D\uDD25" + ellipsis,
      shouldPrivateBeShown: true,
      expectedPrivateLabelContents: "Search with " + otherPrivateDefault.name,
      changePrivateDefaultEngine: true,
    },
  ];

  for (let test of tests) {
    if (test.changePrivateDefaultEngine) {
      await Services.search.setDefaultPrivate(
        otherPrivateDefault,
        Ci.nsISearchService.CHANGE_REASON_UNKNOWN
      );
    }

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [{ selectElement: test.isSelected ? test.id : null }],
      async function(arg) {
        let selection = content.getSelection();
        selection.removeAllRanges();

        if (arg.selectElement) {
          selection.selectAllChildren(
            content.document.getElementById(arg.selectElement)
          );
        }
      }
    );

    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contentAreaContextMenu,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#" + test.id,
      { type: "contextmenu", button: 2 },
      gBrowser.selectedBrowser
    );
    await popupShownPromise;

    let menuItem = document.getElementById("context-searchselect");
    is(
      menuItem.hidden,
      !test.shouldBeShown,
      "search context menu item is shown for  '#" +
        test.id +
        "' and selected is '" +
        test.isSelected +
        "'"
    );

    if (test.shouldBeShown) {
      ok(
        menuItem.label.includes(test.expectedLabelContents),
        "Menu item text '" +
          menuItem.label +
          "' contains the correct search terms '" +
          test.expectedLabelContents +
          "'"
      );
    }

    menuItem = document.getElementById("context-searchselect-private");
    is(
      menuItem.hidden,
      !test.shouldPrivateBeShown,
      "private search context menu item is shown for  '#" + test.id + "' "
    );

    if (test.shouldPrivateBeShown) {
      ok(
        menuItem.label.includes(test.expectedPrivateLabelContents),
        "Menu item text '" +
          menuItem.label +
          "' contains the correct search terms '" +
          test.expectedPrivateLabelContents +
          "'"
      );
    }

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contentAreaContextMenu,
      "popuphidden"
    );
    contentAreaContextMenu.hidePopup();
    await popupHiddenPromise;

    if (test.changePrivateDefaultEngine) {
      await Services.search.setDefaultPrivate(
        originalPrivateDefault,
        Ci.nsISearchService.CHANGE_REASON_UNKNOWN
      );
    }
  }

  // Cleanup.
  gBrowser.removeCurrentTab();
});
