/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(function*() {
  // Clean up when the test finishes.
  yield task_resetState();
});

/**
 * Make sure the downloads button and indicator overflows into the nav-bar
 * chevron properly, and then when those buttons are clicked in the overflow
 * panel that the downloads panel anchors to the chevron.
 */
add_task(function* test_overflow_anchor() {
  // Ensure that state is reset in case previous tests didn't finish.
  yield task_resetState();

  // Record the original width of the window so we can put it back when
  // this test finishes.
  let oldWidth = window.outerWidth;

  // The downloads button should not be overflowed to begin with.
  let button = CustomizableUI.getWidget("downloads-button")
                             .forWindow(window);
  ok(!button.overflowed, "Downloads button should not be overflowed.");

  // Hack - we lock the size of the default flex-y items in the nav-bar,
  // namely, the URL and search inputs. That way we can resize the
  // window without worrying about them flexing.
  const kFlexyItems = ["urlbar-container", "search-container"];
  registerCleanupFunction(() => unlockWidth(kFlexyItems));
  lockWidth(kFlexyItems);

  // Resize the window to half of its original size. That should
  // be enough to overflow the downloads button.
  window.resizeTo(oldWidth / 2, window.outerHeight);
  yield waitForOverflowed(button, true);

  let promise = promisePanelOpened();
  button.node.doCommand();
  yield promise;

  let panel = DownloadsPanel.panel;
  let chevron = document.getElementById("nav-bar-overflow-button");
  is(panel.anchorNode, chevron, "Panel should be anchored to the chevron.");

  DownloadsPanel.hidePanel();

  // Unlock the widths on the flex-y items.
  unlockWidth(kFlexyItems);

  // Put the window back to its original dimensions.
  window.resizeTo(oldWidth, window.outerHeight);

  // The downloads button should eventually be un-overflowed.
  yield waitForOverflowed(button, false);

  // Now try opening the panel again.
  promise = promisePanelOpened();
  button.node.doCommand();
  yield promise;

  is(panel.anchorNode.id, "downloads-indicator-anchor");

  DownloadsPanel.hidePanel();
});

/**
 * For some node IDs, finds the nodes and sets their min-width's to their
 * current width, preventing them from flex-shrinking.
 *
 * @param aItemIDs an array of item IDs to set min-width on.
 */
function lockWidth(aItemIDs) {
  for (let itemID of aItemIDs) {
    let item = document.getElementById(itemID);
    let curWidth = item.getBoundingClientRect().width + "px";
    item.style.minWidth = curWidth;
  }
}

/**
 * Clears the min-width's set on a set of IDs by lockWidth.
 *
 * @param aItemIDs an array of ItemIDs to remove min-width on.
 */
function unlockWidth(aItemIDs) {
  for (let itemID of aItemIDs) {
    let item = document.getElementById(itemID);
    item.style.minWidth = "";
  }
}

/**
 * Waits for a node to enter or exit the overflowed state.
 *
 * @param aItem the node to wait for.
 * @param aIsOverflowed if we're waiting for the item to be overflowed.
 */
function waitForOverflowed(aItem, aIsOverflowed) {
  let deferOverflow = Promise.defer();
  if (aItem.overflowed == aIsOverflowed) {
    return deferOverflow.resolve();
  }

  let observer = new MutationObserver(function(aMutations) {
    if (aItem.overflowed == aIsOverflowed) {
      observer.disconnect();
      deferOverflow.resolve();
    }
  });
  observer.observe(aItem.node, {attributes: true});

  return deferOverflow.promise;
}
