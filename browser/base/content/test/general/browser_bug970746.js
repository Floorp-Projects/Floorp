/* Make sure context menu includes option to search hyperlink text on search engine */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let doc = gBrowser.contentDocument;
    let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
    let ellipsis = "\u2026";

    // Tests if the "Search <engine> for '<some terms>'" context menu item is shown for the
    // given query string of an element. Tests to make sure label includes the proper search terms.
    //
    // Options:
    // 
    //   id: The id of the element to test.
    //   isSelected: Flag to enable selection (text hilight) the contents of the element
    //   shouldBeShown: The display state of the menu item
    //   expectedLabelContents: The menu item label should contain a portion of this string.
    //                          Will only be tested if shouldBeShown is true.

    let testElement = function(opts) {
      let element = doc.getElementById(opts.id);
      document.popupNode = element;

      let selection = content.getSelection();
      selection.removeAllRanges();

      if(opts.isSelected) {
        selection.selectAllChildren(element);
      }

      let contextMenu = new nsContextMenu(contentAreaContextMenu);
      let menuItem = document.getElementById("context-searchselect");

      is(document.getElementById("context-searchselect").hidden, !opts.shouldBeShown, "search context menu item is shown for  '#" + opts.id + "' and selected is '" + opts.isSelected + "'");

      if(opts.shouldBeShown) {
        ok(menuItem.label.contains(opts.expectedLabelContents), "Menu item text '" + menuItem.label  + "' contains the correct search terms '" + opts.expectedLabelContents  + "'");
      }
    }

    testElement({
      id: "link",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm a link!",
    });
    testElement({
      id: "link",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "I'm a link!",
    });

    testElement({
      id: "longLink",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm a really lo" + ellipsis,
    });
    testElement({
      id: "longLink",
      isSelected: false,
      shouldBeShown: true,
      expectedLabelContents: "I'm a really lo" + ellipsis,
    });

    testElement({
      id: "plainText",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "Right clicking " + ellipsis,
    });
    testElement({
      id: "plainText",
      isSelected: false,
      shouldBeShown: false,
    });

    testElement({
      id: "mixedContent",
      isSelected: true,
      shouldBeShown: true,
      expectedLabelContents: "I'm some text, " + ellipsis,
    });
    testElement({
      id: "mixedContent",
      isSelected: false,
      shouldBeShown: false,
    });

    // cleanup
    document.popupNode = null;
    gBrowser.removeCurrentTab();
    finish();
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/base/content/test/general/browser_bug970746.xhtml";
}