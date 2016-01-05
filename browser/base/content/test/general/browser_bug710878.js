/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE = "data:text/html;charset=utf-8,<a href='%23xxx'><span>word1 <span> word2 </span></span><span> word3</span></a>";

/**
 * Tests that we correctly compute the text for context menu
 * selection of some content.
 */
add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu,
                                                          "popupshown");
      let awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu,
                                                           "popuphidden");

      yield BrowserTestUtils.synthesizeMouseAtCenter("a", {
        type: "contextmenu",
        button: 2,
      }, browser);

      yield awaitPopupShown;

      is(gContextMenu.linkTextStr, "word1 word2 word3",
         "Text under link is correctly computed.");

      contextMenu.hidePopup();
      yield awaitPopupHidden;
  });
});
