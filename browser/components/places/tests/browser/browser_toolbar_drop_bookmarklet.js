/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const sandbox = sinon.createSandbox();

const URL1 = "https://example.com/1/";
const URL2 = "https://example.com/2/";
const BOOKMARKLET_URL = `javascript: (() => {alert('Hello, World!');})();`;
let bookmarks;

registerCleanupFunction(async function () {
  sandbox.restore();
});

add_task(async function test() {
  // Make sure the bookmarks bar is visible and restore its state on cleanup.
  let toolbar = document.getElementById("PersonalToolbar");
  ok(toolbar, "PersonalToolbar should not be null");

  await PlacesUtils.bookmarks.eraseEverything();

  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function () {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }
  // Setup the node we will use to be dropped. The actual node used does not
  // matter because we will set its data, effect, and mimeType manually.
  let placesItems = document.getElementById("PlacesToolbarItems");
  Assert.ok(placesItems, "PlacesToolbarItems should not be null");

  /**
   * Simulates a drop of a bookmarklet URI onto the bookmarks bar.
   *
   * @param {string} aEffect
   *        The effect to use for the drop operation: move, copy, or link.
   */
  let simulateDragDrop = async function (aEffect) {
    info("Simulates drag/drop of a new javascript:URL to the bookmarks");
    await withBookmarksDialog(
      true,
      function openDialog() {
        EventUtils.synthesizeDrop(
          toolbar,
          placesItems,
          [[{ type: "text/x-moz-url", data: BOOKMARKLET_URL }]],
          aEffect,
          window
        );
      },
      async function testNameField(dialogWin) {
        info("Checks that there is a javascript:URL in ShowBookmarksDialog");

        let location = dialogWin.document.getElementById(
          "editBMPanel_locationField"
        ).value;

        Assert.equal(
          location,
          BOOKMARKLET_URL,
          "Should have opened the ShowBookmarksDialog with the correct bookmarklet url to be bookmarked"
        );
      }
    );

    info("Simulates drag/drop of a new URL to the bookmarks");
    let spy = sandbox
      .stub(PlacesUIUtils, "showBookmarkDialog")
      .returns(Promise.resolve());

    let promise = PlacesTestUtils.waitForNotification(
      "bookmark-added",
      events => events.some(({ url }) => url == URL1)
    );

    EventUtils.synthesizeDrop(
      toolbar,
      placesItems,
      [[{ type: "text/x-moz-url", data: URL1 }]],
      aEffect,
      window
    );

    await promise;
    Assert.ok(spy.notCalled, "ShowBookmarksDialog on drop not called for url");
    sandbox.restore();
  };

  let effects = ["copy", "link"];
  for (let effect of effects) {
    await simulateDragDrop(effect);
  }

  info("Move of existing bookmark / bookmarklet on toolbar");
  // Clean previous bookmarks to ensure right ids count.
  await PlacesUtils.bookmarks.eraseEverything();

  info("Insert list of bookamrks to have bookmarks (ids) for moving");
  // Ensure bookmarks are visible on the toolbar.
  let promiseBookmarksOnToolbar = BrowserTestUtils.waitForMutationCondition(
    placesItems,
    { childList: true },
    () => placesItems.childNodes.length == 3
  );
  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        title: "bm1",
        url: URL1,
      },
      {
        title: "bm2",
        url: URL2,
      },
      {
        title: "bookmarklet",
        url: BOOKMARKLET_URL,
      },
    ],
  });
  await promiseBookmarksOnToolbar;

  let spy = sandbox
    .stub(PlacesUIUtils, "showBookmarkDialog")
    .returns(Promise.resolve());

  info("Moving existing Bookmark from position [1] to [0] on Toolbar");
  let urlMoveNotification = PlacesTestUtils.waitForNotification(
    "bookmark-moved",
    events =>
      events.some(
        e =>
          e.parentGuid === PlacesUtils.bookmarks.toolbarGuid &&
          e.oldParentGuid === PlacesUtils.bookmarks.toolbarGuid &&
          e.oldIndex == 1 &&
          e.index == 0
      )
  );

  EventUtils.synthesizeDrop(
    placesItems,
    placesItems.childNodes[0],
    [
      [
        {
          type: "text/x-moz-place",
          data: PlacesUtils.wrapNode(
            placesItems.childNodes[1]._placesNode,
            "text/x-moz-place"
          ),
        },
      ],
    ],
    "move",
    window
  );

  await urlMoveNotification;
  Assert.ok(spy.notCalled, "ShowBookmarksDialog not called on move for url");

  info("Moving existing Bookmarklet from position [2] to [1] on Toolbar");
  let bookmarkletMoveNotificatio = PlacesTestUtils.waitForNotification(
    "bookmark-moved",
    events =>
      events.some(
        e =>
          e.parentGuid === PlacesUtils.bookmarks.toolbarGuid &&
          e.oldParentGuid === PlacesUtils.bookmarks.toolbarGuid &&
          e.oldIndex == 2 &&
          e.index == 1
      )
  );

  EventUtils.synthesizeDrop(
    toolbar,
    placesItems.childNodes[1],
    [
      [
        {
          type: "text/x-moz-place",
          data: PlacesUtils.wrapNode(
            placesItems.childNodes[2]._placesNode,
            "text/x-moz-place"
          ),
        },
      ],
    ],
    "move",
    window
  );

  await bookmarkletMoveNotificatio;
  Assert.ok(spy.notCalled, "ShowBookmarksDialog not called on move for url");
  sandbox.restore();
});
