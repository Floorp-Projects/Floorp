ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "gFluentStrings", function() {
  return new Localization(["branding/brand.ftl", "browser/browser.ftl"], true);
});

function openLibrary(callback, aLeftPaneRoot) {
  let library = window.openDialog(
    "chrome://browser/content/places/places.xhtml",
    "",
    "chrome,toolbar=yes,dialog=no,resizable",
    aLeftPaneRoot
  );
  waitForFocus(function() {
    checkLibraryPaneVisibility(library, aLeftPaneRoot);
    callback(library);
  }, library);

  return library;
}

/**
 * Returns a handle to a Library window.
 * If one is opens returns itm otherwise it opens a new one.
 *
 * @param aLeftPaneRoot
 *        Hierarchy to open and select in the left pane.
 */
function promiseLibrary(aLeftPaneRoot) {
  return new Promise(resolve => {
    let library = Services.wm.getMostRecentWindow("Places:Organizer");
    if (library && !library.closed) {
      if (aLeftPaneRoot) {
        library.PlacesOrganizer.selectLeftPaneContainerByHierarchy(
          aLeftPaneRoot
        );
      }
      checkLibraryPaneVisibility(library, aLeftPaneRoot);
      resolve(library);
    } else {
      openLibrary(resolve, aLeftPaneRoot);
    }
  });
}

function promiseLibraryClosed(organizer) {
  return new Promise(resolve => {
    if (organizer.closed) {
      resolve();
      return;
    }
    // Wait for the Organizer window to actually be closed
    organizer.addEventListener(
      "unload",
      function() {
        executeSoon(resolve);
      },
      { once: true }
    );

    // Close Library window.
    organizer.close();
  });
}

function checkLibraryPaneVisibility(library, selectedPane) {
  // Make sure right view is shown
  if (selectedPane == "Downloads") {
    Assert.ok(
      library.ContentTree.view.hidden,
      "Bookmark/History tree is hidden"
    );
    Assert.ok(
      !library.document.getElementById("downloadsRichListBox").hidden,
      "Downloads are shown"
    );
  } else {
    Assert.ok(
      !library.ContentTree.view.hidden,
      "Bookmark/History tree is shown"
    );
    Assert.ok(
      library.document.getElementById("downloadsRichListBox").hidden,
      "Downloads are hidden"
    );
  }

  // Check currentView getter
  Assert.ok(!library.ContentArea.currentView.hidden, "Current view is shown");
}

/**
 * Waits for a clipboard operation to complete, looking for the expected type.
 *
 * @see waitForClipboard
 *
 * @param aPopulateClipboardFn
 *        Function to populate the clipboard.
 * @param aFlavor
 *        Data flavor to expect.
 */
function promiseClipboard(aPopulateClipboardFn, aFlavor) {
  return new Promise((resolve, reject) => {
    waitForClipboard(
      data => !!data,
      aPopulateClipboardFn,
      resolve,
      reject,
      aFlavor
    );
  });
}

function synthesizeClickOnSelectedTreeCell(aTree, aOptions) {
  if (aTree.view.selection.count < 1) {
    throw new Error("The test node should be successfully selected");
  }
  // Get selection rowID.
  let min = {},
    max = {};
  aTree.view.selection.getRangeAt(0, min, max);
  let rowID = min.value;
  aTree.ensureRowIsVisible(rowID);
  // Calculate the click coordinates.
  var rect = aTree.getCoordsForCellItem(rowID, aTree.columns[0], "text");
  var x = rect.x + rect.width / 2;
  var y = rect.y + rect.height / 2;
  // Simulate the click.
  EventUtils.synthesizeMouse(
    aTree.body,
    x,
    y,
    aOptions || {},
    aTree.ownerGlobal
  );
}

/**
 * Makes the specified toolbar visible or invisible and returns a Promise object
 * that is resolved when the toolbar has completed any animations associated
 * with hiding or showing the toolbar.
 *
 * Note that this code assumes that changes to a toolbar's visibility trigger
 * a transition on the max-height property of the toolbar element.
 * Changes to this styling could cause the returned Promise object to be
 * resolved too early or not at all.
 *
 * @param aToolbar
 *        The toolbar to update.
 * @param aVisible
 *        True to make the toolbar visible, false to make it hidden.
 *
 * @return {Promise}
 * @resolves Any animation associated with updating the toolbar's visibility has
 *           finished.
 * @rejects Never.
 */
function promiseSetToolbarVisibility(aToolbar, aVisible, aCallback) {
  if (isToolbarVisible(aToolbar) != aVisible) {
    let visibilityChanged = TestUtils.waitForCondition(
      () => aToolbar.collapsed != aVisible
    );
    setToolbarVisibility(aToolbar, aVisible, undefined, false);
    return visibilityChanged;
  }
  return Promise.resolve();
}

/**
 * Helper function to determine if the given toolbar is in the visible
 * state according to its autohide/collapsed attribute.
 *
 * @aToolbar The toolbar to query.
 *
 * @returns True if the relevant attribute on |aToolbar| indicates it is
 *          visible, false otherwise.
 */
function isToolbarVisible(aToolbar) {
  let hidingAttribute =
    aToolbar.getAttribute("type") == "menubar" ? "autohide" : "collapsed";
  let hidingValue = aToolbar.getAttribute(hidingAttribute).toLowerCase();
  // Check for both collapsed="true" and collapsed="collapsed"
  return hidingValue !== "true" && hidingValue !== hidingAttribute;
}

/**
 * Executes a task after opening the bookmarks dialog, then cancels the dialog.
 *
 * @param autoCancel
 *        whether to automatically cancel the dialog at the end of the task
 * @param openFn
 *        generator function causing the dialog to open
 * @param taskFn
 *        the task to execute once the dialog is open
 * @param closeFn
 *        A function to be used to wait for pending work when the dialog is
 *        closing. It is passed the dialog window handle and should return a promise.
 */
var withBookmarksDialog = async function(
  autoCancel,
  openFn,
  taskFn,
  closeFn,
  dialogUrl = "chrome://browser/content/places/bookmarkProperties",
  skipOverlayWait = false
) {
  let closed = false;
  let protonModal =
    Services.prefs.getBoolPref("browser.proton.modals.enabled", false) &&
    // We can't show the in-window prompt for windows which don't have
    // gDialogBox, like the library (Places:Organizer) window.
    Services.wm.getMostRecentWindow("").gDialogBox;
  let dialogPromise;
  if (protonModal) {
    dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(null, dialogUrl, {
      isSubDialog: true,
    });
  } else {
    dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded(null, win => {
      return win.document.documentURI.startsWith(dialogUrl);
    }).then(win => {
      ok(
        win.location.href.startsWith(dialogUrl),
        "The bookmark properties dialog is open: " + win.location.href
      );
      // This is needed for the overlay.
      return SimpleTest.promiseFocus(win).then(() => win);
    });
  }
  let dialogClosePromise = dialogPromise.then(win => {
    if (!protonModal) {
      return BrowserTestUtils.domWindowClosed(win);
    }
    let container = win.top.document.getElementById("window-modal-dialog");
    return BrowserTestUtils.waitForEvent(container, "close").then(() => {
      return BrowserTestUtils.waitForMutationCondition(
        container,
        { childList: true, attributes: true },
        () => !container.hasChildNodes() && !container.open
      );
    });
  });
  dialogClosePromise.then(() => {
    closed = true;
  });

  info("withBookmarksDialog: opening the dialog");
  // The dialog might be modal and could block our events loop, so executeSoon.
  executeSoon(openFn);

  info("withBookmarksDialog: waiting for the dialog");
  let dialogWin = await dialogPromise;

  // Ensure overlay is loaded
  if (!skipOverlayWait) {
    info("waiting for the overlay to be loaded");
    await waitForCondition(
      () => dialogWin.gEditItemOverlay.initialized,
      "EditItemOverlay should be initialized"
    );
  }

  // Check the first input is focused.
  let doc = dialogWin.document;
  let elt = doc.querySelector("vbox:not([collapsed=true]) > input");
  ok(elt, "There should be an input to focus.");

  if (elt) {
    info("waiting for focus on the first textfield");
    await waitForCondition(
      () => doc.activeElement == elt,
      "The first non collapsed input should have been focused"
    );
  }

  info("withBookmarksDialog: executing the task");

  let closePromise = () => Promise.resolve();
  if (closeFn) {
    closePromise = closeFn(dialogWin);
  }

  try {
    await taskFn(dialogWin);
  } finally {
    if (!closed && autoCancel) {
      info("withBookmarksDialog: canceling the dialog");
      doc.getElementById("bookmarkpropertiesdialog").cancelDialog();
      await closePromise;
    }
    // Give the dialog a little time to close itself.
    await dialogClosePromise;
  }
};

/**
 * Opens the contextual menu on the element pointed by the given selector.
 *
 * @param selector
 *        Valid selector syntax
 * @return Promise
 *         Returns a Promise that resolves once the context menu has been
 *         opened.
 */
var openContextMenuForContentSelector = async function(browser, selector) {
  info("wait for the context menu");
  let contextPromise = BrowserTestUtils.waitForEvent(
    document.getElementById("contentAreaContextMenu"),
    "popupshown"
  );
  await SpecialPowers.spawn(browser, [{ selector }], async function(args) {
    let doc = content.document;
    let elt = doc.querySelector(args.selector);
    dump(`openContextMenuForContentSelector: found ${elt}\n`);

    /* Open context menu so chrome can access the element */
    const domWindowUtils = content.windowUtils;
    let rect = elt.getBoundingClientRect();
    let left = rect.left + rect.width / 2;
    let top = rect.top + rect.height / 2;
    domWindowUtils.sendMouseEvent(
      "contextmenu",
      left,
      top,
      2,
      1,
      0,
      false,
      0,
      0,
      true
    );
  });
  await contextPromise;
};

/**
 * Waits for a specified condition to happen.
 *
 * @param conditionFn
 *        a Function or a generator function, returning a boolean for whether
 *        the condition is fulfilled.
 * @param errorMsg
 *        Error message to use if the condition has not been satisfied after a
 *        meaningful amount of tries.
 */
var waitForCondition = async function(conditionFn, errorMsg) {
  for (let tries = 0; tries < 100; ++tries) {
    if (await conditionFn()) {
      return;
    }
    await new Promise(resolve => {
      if (!waitForCondition._timers) {
        waitForCondition._timers = new Set();
        registerCleanupFunction(() => {
          is(
            waitForCondition._timers.size,
            0,
            "All the wait timers have been removed"
          );
          delete waitForCondition._timers;
        });
      }
      let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      waitForCondition._timers.add(timer);
      timer.init(
        () => {
          waitForCondition._timers.delete(timer);
          resolve();
        },
        100,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    });
  }
  ok(false, errorMsg);
};

/**
 * Fills a bookmarks dialog text field ensuring to cause expected edit events.
 *
 * @param id
 *        id of the text field
 * @param text
 *        text to fill in
 * @param win
 *        dialog window
 * @param [optional] blur
 *        whether to blur at the end.
 */
function fillBookmarkTextField(id, text, win, blur = true) {
  let elt = win.document.getElementById(id);
  elt.focus();
  elt.select();
  if (!text) {
    EventUtils.synthesizeKey("VK_DELETE", {}, win);
  } else {
    for (let c of text.split("")) {
      EventUtils.synthesizeKey(c, {}, win);
    }
  }
  if (blur) {
    elt.blur();
  }
}

/**
 * Executes a task after opening the bookmarks or history sidebar. Takes care
 * of closing the sidebar once done.
 *
 * @param type
 *        either "bookmarks" or "history".
 * @param taskFn
 *        The task to execute once the sidebar is ready. Will get the Places
 *        tree view as input.
 */
var withSidebarTree = async function(type, taskFn) {
  let sidebar = document.getElementById("sidebar");
  info("withSidebarTree: waiting sidebar load");
  let sidebarLoadedPromise = new Promise(resolve => {
    sidebar.addEventListener(
      "load",
      function() {
        executeSoon(resolve);
      },
      { capture: true, once: true }
    );
  });
  let sidebarId =
    type == "bookmarks" ? "viewBookmarksSidebar" : "viewHistorySidebar";
  SidebarUI.show(sidebarId);
  await sidebarLoadedPromise;

  let treeId = type == "bookmarks" ? "bookmarks-view" : "historyTree";
  let tree = sidebar.contentDocument.getElementById(treeId);

  // Need to executeSoon since the tree is initialized on sidebar load.
  info("withSidebarTree: executing the task");
  try {
    await taskFn(tree);
  } finally {
    SidebarUI.hide();
  }
};

/**
 * Executes a task after opening the Library on a given root. Takes care
 * of closing the library once done.
 *
 * @param hierarchy
 *        The left pane hierarchy to open.
 * @param taskFn
 *        The task to execute once the Library is ready.
 *        Will get { left, right } trees as argument.
 */
var withLibraryWindow = async function(hierarchy, taskFn) {
  let library = await promiseLibrary(hierarchy);
  let left = library.document.getElementById("placesList");
  let right = library.document.getElementById("placeContent");
  info("withLibrary: executing the task");
  try {
    await taskFn({ left, right });
  } finally {
    await promiseLibraryClosed(library);
  }
};

function promisePlacesInitComplete() {
  const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
    Ci.nsIObserver
  );

  let placesInitCompleteObserved = TestUtils.topicObserved(
    "places-browser-init-complete"
  );

  gBrowserGlue.observe(
    null,
    "browser-glue-test",
    "places-browser-init-complete"
  );

  return placesInitCompleteObserved;
}

// Function copied from browser/base/content/test/general/head.js.
function promisePopupShown(popup) {
  return new Promise(resolve => {
    if (popup.state == "open") {
      resolve();
    } else {
      let onPopupShown = event => {
        popup.removeEventListener("popupshown", onPopupShown);
        resolve();
      };
      popup.addEventListener("popupshown", onPopupShown);
    }
  });
}

// Function copied from browser/base/content/test/general/head.js.
function promisePopupHidden(popup) {
  return new Promise(resolve => {
    let onPopupHidden = event => {
      popup.removeEventListener("popuphidden", onPopupHidden);
      resolve();
    };
    popup.addEventListener("popuphidden", onPopupHidden);
  });
}

// Identify a bookmark node in the Bookmarks Toolbar by its guid.
function getToolbarNodeForItemGuid(itemGuid) {
  let children = document.getElementById("PlacesToolbarItems").childNodes;
  for (let child of children) {
    if (itemGuid === child._placesNode.bookmarkGuid) {
      return child;
    }
  }
  return null;
}

// Open the bookmarks Star UI by clicking the star button on the address bar.
async function clickBookmarkStar(win = window) {
  let shownPromise = promisePopupShown(
    win.document.getElementById("editBookmarkPanel")
  );
  win.BookmarkingUI.star.click();
  await shownPromise;
}

// Close the bookmarks Star UI by clicking the "Done" button.
async function hideBookmarksPanel(win = window) {
  let hiddenPromise = promisePopupHidden(
    win.document.getElementById("editBookmarkPanel")
  );
  // Confirm and close the dialog.
  win.document.getElementById("editBookmarkPanelDoneButton").click();
  await hiddenPromise;
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.bookmarks.defaultLocation");
});
