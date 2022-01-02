"use strict";

// This is a test for PageActions.jsm, specifically the generalized parts that
// add and remove page actions and toggle them in the urlbar.  This does not
// test the built-in page actions; browser_page_action_menu.js does that.

// Initialization.  Must run first.
add_task(async function init() {
  await initPageActionsTest();
});

// Makes sure that urlbar nodes appear in the correct order in a new window.
add_task(async function urlbarOrderNewWindow() {
  // Make some new actions.
  let actions = [0, 1, 2].map(i => {
    return PageActions.addAction(
      new PageActions.Action({
        id: `test-urlbarOrderNewWindow-${i}`,
        title: `Test urlbarOrderNewWindow ${i}`,
        pinnedToUrlbar: true,
      })
    );
  });

  // Make sure PageActions knows they're inserted before the bookmark action in
  // the urlbar.
  Assert.deepEqual(
    PageActions._persistedActions.idsInUrlbar.slice(
      PageActions._persistedActions.idsInUrlbar.length - (actions.length + 1)
    ),
    actions.map(a => a.id).concat([PageActions.ACTION_ID_BOOKMARK]),
    "PageActions._persistedActions.idsInUrlbar has new actions inserted"
  );
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window)
      .slice(PageActions.actionsInUrlbar(window).length - (actions.length + 1))
      .map(a => a.id),
    actions.map(a => a.id).concat([PageActions.ACTION_ID_BOOKMARK]),
    "PageActions.actionsInUrlbar has new actions inserted"
  );

  // Reach into _persistedActions to move the new actions to the front of the
  // urlbar, same as if the user moved them.  That way we can test that insert-
  // before IDs are correctly non-null when the urlbar nodes are inserted in the
  // new window below.
  PageActions._persistedActions.idsInUrlbar.splice(
    PageActions._persistedActions.idsInUrlbar.length - (actions.length + 1),
    actions.length
  );
  for (let i = 0; i < actions.length; i++) {
    PageActions._persistedActions.idsInUrlbar.splice(i, 0, actions[i].id);
  }

  // Save the right-ordered IDs to use below, just in case they somehow get
  // changed when the new window opens, which shouldn't happen, but maybe
  // there's bugs.
  let ids = PageActions._persistedActions.idsInUrlbar.slice();

  // Make sure that worked.
  Assert.deepEqual(
    ids.slice(0, actions.length),
    actions.map(a => a.id),
    "PageActions._persistedActions.idsInUrlbar now has new actions at front"
  );

  // _persistedActions will contain the IDs of test actions added and removed
  // above (unless PageActions._purgeUnregisteredPersistedActions() was called
  // for all of them, which it wasn't).  Filter them out because they should
  // not appear in the new window (or any window at this point).
  ids = ids.filter(id => PageActions.actionForID(id));

  // Open the new window.
  let win = await BrowserTestUtils.openNewBrowserWindow();

  // Collect its urlbar nodes.
  let actualUrlbarNodeIDs = [];
  for (
    let node = win.BrowserPageActions.mainButtonNode.nextElementSibling;
    node;
    node = node.nextElementSibling
  ) {
    actualUrlbarNodeIDs.push(node.id);
  }

  // Now check that they're in the right order.
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    ids.map(id => win.BrowserPageActions.urlbarButtonNodeIDForActionID(id)),
    "Expected actions in new window's urlbar"
  );

  // Done, clean up.
  await BrowserTestUtils.closeWindow(win);
  for (let action of actions) {
    action.remove();
  }
});

// Stores version-0 (unversioned actually) persisted actions and makes sure that
// migrating to version 1 works.
add_task(async function migrate1() {
  // Add a test action so we can test a non-built-in action below.
  let actionId = "test-migrate1";
  PageActions.addAction(
    new PageActions.Action({
      id: actionId,
      title: "Test migrate1",
      pinnedToUrlbar: true,
    })
  );

  // Add the bookmark action first to make sure it ends up last after migration.
  // Also include a non-default action to make sure we're not accidentally
  // testing default behavior.
  let ids = [PageActions.ACTION_ID_BOOKMARK, actionId];
  let persisted = ids.reduce(
    (memo, id) => {
      memo.ids[id] = true;
      memo.idsInUrlbar.push(id);
      return memo;
    },
    { ids: {}, idsInUrlbar: [] }
  );

  Services.prefs.setStringPref(
    PageActions.PREF_PERSISTED_ACTIONS,
    JSON.stringify(persisted)
  );

  // Migrate.
  PageActions._loadPersistedActions();

  Assert.equal(PageActions._persistedActions.version, 1, "Correct version");

  // expected order
  let orderedIDs = [actionId, PageActions.ACTION_ID_BOOKMARK];

  // Check the ordering.
  Assert.deepEqual(
    PageActions._persistedActions.idsInUrlbar,
    orderedIDs,
    "PageActions._persistedActions.idsInUrlbar has right order"
  );
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window).map(a => a.id),
    orderedIDs,
    "PageActions.actionsInUrlbar has right order"
  );

  // Open a new window.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: "http://example.com/",
  });

  // Collect its urlbar nodes.
  let actualUrlbarNodeIDs = [];
  for (
    let node = win.BrowserPageActions.mainButtonNode.nextElementSibling;
    node;
    node = node.nextElementSibling
  ) {
    actualUrlbarNodeIDs.push(node.id);
  }

  // Now check that they're in the right order.
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    orderedIDs.map(id =>
      win.BrowserPageActions.urlbarButtonNodeIDForActionID(id)
    ),
    "Expected actions in new window's urlbar"
  );

  // Done, clean up.
  await BrowserTestUtils.closeWindow(win);
  Services.prefs.clearUserPref(PageActions.PREF_PERSISTED_ACTIONS);
  PageActions.actionForID(actionId).remove();
});

// Opens a new browser window and makes sure per-window state works right.
add_task(async function perWindowState() {
  // Add a test action.
  let title = "Test perWindowState";
  let action = PageActions.addAction(
    new PageActions.Action({
      iconURL: "chrome://browser/skin/mail.svg",
      id: "test-perWindowState",
      pinnedToUrlbar: true,
      title,
    })
  );

  let actionsInUrlbar = PageActions.actionsInUrlbar(window);

  // Open a new browser window and load an actionable page so that the action
  // shows up in it.
  let newWindow = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: newWindow.gBrowser,
    url: "http://example.com/",
  });

  // Set a new title globally.
  let newGlobalTitle = title + " new title";
  action.setTitle(newGlobalTitle);
  Assert.equal(action.getTitle(), newGlobalTitle, "Title: global");
  Assert.equal(action.getTitle(window), newGlobalTitle, "Title: old window");
  Assert.equal(action.getTitle(newWindow), newGlobalTitle, "Title: new window");

  // Initialize panel nodes in the windows
  document.getElementById("pageActionButton").click();
  await BrowserTestUtils.waitForEvent(document, "popupshowing", true);
  newWindow.document.getElementById("pageActionButton").click();
  await BrowserTestUtils.waitForEvent(newWindow.document, "popupshowing", true);

  // The action's panel button nodes should be updated in both windows.
  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(
    action.id
  );
  for (let win of [window, newWindow]) {
    win.BrowserPageActions.placeLazyActionsInPanel();
    let panelButtonNode = win.document.getElementById(panelButtonID);
    Assert.equal(
      panelButtonNode.getAttribute("label"),
      newGlobalTitle,
      "Panel button label should be global title"
    );
  }

  // Set a new title in the new window.
  let newPerWinTitle = title + " new title in new window";
  action.setTitle(newPerWinTitle, newWindow);
  Assert.equal(
    action.getTitle(),
    newGlobalTitle,
    "Title: global should remain same"
  );
  Assert.equal(
    action.getTitle(window),
    newGlobalTitle,
    "Title: old window should remain same"
  );
  Assert.equal(
    action.getTitle(newWindow),
    newPerWinTitle,
    "Title: new window should be new"
  );

  // The action's panel button node should be updated in the new window but the
  // same in the old window.
  let panelButtonNode1 = document.getElementById(panelButtonID);
  Assert.equal(
    panelButtonNode1.getAttribute("label"),
    newGlobalTitle,
    "Panel button label in old window"
  );
  let panelButtonNode2 = newWindow.document.getElementById(panelButtonID);
  Assert.equal(
    panelButtonNode2.getAttribute("label"),
    newPerWinTitle,
    "Panel button label in new window"
  );

  // Disable the action in the new window.
  action.setDisabled(true, newWindow);
  Assert.equal(
    action.getDisabled(),
    false,
    "Disabled: global should remain false"
  );
  Assert.equal(
    action.getDisabled(window),
    false,
    "Disabled: old window should remain false"
  );
  Assert.equal(
    action.getDisabled(newWindow),
    true,
    "Disabled: new window should be true"
  );

  // Check PageActions.actionsInUrlbar for each window.
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window).map(a => a.id),
    actionsInUrlbar.map(a => a.id),
    "PageActions.actionsInUrlbar: old window should have all actions in urlbar"
  );
  Assert.deepEqual(
    PageActions.actionsInUrlbar(newWindow).map(a => a.id),
    actionsInUrlbar.map(a => a.id).filter(id => id != action.id),
    "PageActions.actionsInUrlbar: new window should have all actions in urlbar except the test action"
  );

  // Check the urlbar nodes for the old window.
  let actualUrlbarNodeIDs = [];
  for (
    let node = BrowserPageActions.mainButtonNode.nextElementSibling;
    node;
    node = node.nextElementSibling
  ) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    actionsInUrlbar.map(a =>
      BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)
    ),
    "Old window should have all nodes in urlbar"
  );

  // Check the urlbar nodes for the new window.
  actualUrlbarNodeIDs = [];
  for (
    let node = newWindow.BrowserPageActions.mainButtonNode.nextElementSibling;
    node;
    node = node.nextElementSibling
  ) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    actionsInUrlbar
      .filter(a => a.id != action.id)
      .map(a => BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)),
    "New window should have all nodes in urlbar except for the test action's"
  );

  // Done, clean up.
  await BrowserTestUtils.closeWindow(newWindow);
  action.remove();
});

add_task(async function action_disablePrivateBrowsing() {
  let id = "testWidget";
  let action = PageActions.addAction(
    new PageActions.Action({
      id,
      disablePrivateBrowsing: true,
      title: "title",
      disabled: false,
      pinnedToUrlbar: true,
    })
  );
  // Open an actionable page so that the main page action button appears.
  let url = "http://example.com/";
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await BrowserTestUtils.openNewForegroundTab(
    privateWindow.gBrowser,
    url,
    true,
    true
  );

  Assert.ok(action.canShowInWindow(window), "should show in default window");
  Assert.ok(
    !action.canShowInWindow(privateWindow),
    "should not show in private browser"
  );
  Assert.ok(action.shouldShowInUrlbar(window), "should show in default urlbar");
  Assert.ok(
    !action.shouldShowInUrlbar(privateWindow),
    "should not show in default urlbar"
  );
  Assert.ok(action.shouldShowInPanel(window), "should show in default urlbar");
  Assert.ok(
    !action.shouldShowInPanel(privateWindow),
    "should not show in default urlbar"
  );

  action.remove();

  privateWindow.close();
});
