"use strict";

// This is a test for PageActions.jsm, specifically the generalized parts that
// add and remove page actions and toggle them in the urlbar.  This does not
// test the built-in page actions; browser_page_action_menu.js does that.

// Initialization.  Must run first.
add_task(async function init() {
  // The page action urlbar button, and therefore the panel, is only shown when
  // the current tab is actionable -- i.e., a normal web page.  about:blank is
  // not, so open a new tab first thing, and close it when this test is done.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "http://example.com/",
  });
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });

  await initPageActionsTest();
});

// Tests a simple non-built-in action without an iframe or subview.  Also
// thoroughly checks most of the action's properties, methods, and DOM nodes, so
// it's not necessary to do that in general in other test tasks.
add_task(async function simple() {
  let iconURL = "chrome://browser/skin/mail.svg";
  let id = "test-simple";
  let title = "Test simple";
  let tooltip = "Test simple tooltip";

  let onCommandCallCount = 0;
  let onPlacedInPanelCallCount = 0;
  let onPlacedInUrlbarCallCount = 0;
  let onShowingInPanelCallCount = 0;
  let onCommandExpectedButtonID;

  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions.urlbarButtonNodeIDForActionID(id);

  // Open the panel so that actions are added to it, and then close it.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  let initialActions = PageActions.actions;
  let initialActionsInPanel = PageActions.actionsInPanel(window);
  let initialActionsInUrlbar = PageActions.actionsInUrlbar(window);

  let action = PageActions.addAction(
    new PageActions.Action({
      iconURL,
      id,
      title,
      tooltip,
      onCommand(event, buttonNode) {
        onCommandCallCount++;
        Assert.ok(event, "event should be non-null: " + event);
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, onCommandExpectedButtonID, "buttonNode.id");
      },
      onPlacedInPanel(buttonNode) {
        onPlacedInPanelCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, panelButtonID, "buttonNode.id");
      },
      onPlacedInUrlbar(buttonNode) {
        onPlacedInUrlbarCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, urlbarButtonID, "buttonNode.id");
      },
      onShowingInPanel(buttonNode) {
        onShowingInPanelCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, panelButtonID, "buttonNode.id");
      },
    })
  );

  Assert.equal(action.getIconURL(), iconURL, "iconURL");
  Assert.equal(action.id, id, "id");
  Assert.equal(action.pinnedToUrlbar, gProtonUrlbar, "pinnedToUrlbar");
  Assert.equal(action.getDisabled(), false, "disabled");
  Assert.equal(action.getDisabled(window), false, "disabled in window");
  Assert.equal(action.getTitle(), title, "title");
  Assert.equal(action.getTitle(window), title, "title in window");
  Assert.equal(action.getTooltip(), tooltip, "tooltip");
  Assert.equal(action.getTooltip(window), tooltip, "tooltip in window");
  Assert.equal(action.getWantsSubview(), false, "subview");
  Assert.equal(action.getWantsSubview(window), false, "subview in window");
  Assert.equal(action.urlbarIDOverride, null, "urlbarIDOverride");
  Assert.equal(action.wantsIframe, false, "wantsIframe");

  Assert.ok(!("__insertBeforeActionID" in action), "__insertBeforeActionID");
  Assert.ok(!("__isSeparator" in action), "__isSeparator");
  Assert.ok(!("__urlbarNodeInMarkup" in action), "__urlbarNodeInMarkup");
  Assert.ok(!("__transient" in action), "__transient");

  // The action shouldn't be placed in the panel until it opens for the first
  // time.
  Assert.equal(
    onPlacedInPanelCallCount,
    0,
    "onPlacedInPanelCallCount should remain 0"
  );
  Assert.equal(
    onPlacedInUrlbarCallCount,
    gProtonUrlbar ? 1 : 0,
    "onPlacedInUrlbarCallCount after adding the action"
  );
  Assert.equal(
    onShowingInPanelCallCount,
    0,
    "onShowingInPanelCallCount should remain 0"
  );

  // Open the panel so that actions are added to it, and then close it.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  Assert.equal(
    onPlacedInPanelCallCount,
    1,
    "onPlacedInPanelCallCount should be inc'ed"
  );
  Assert.equal(
    onShowingInPanelCallCount,
    1,
    "onShowingInPanelCallCount should be inc'ed"
  );

  // Build an array of the expected actions in the panel and compare it to the
  // actual actions.  Don't assume that there are or aren't already other non-
  // built-in actions.
  let sepIndex = initialActionsInPanel.findIndex(
    a => a.id == PageActions.ACTION_ID_BUILT_IN_SEPARATOR
  );
  let initialSepIndex = sepIndex;
  let indexInPanel;
  if (sepIndex < 0) {
    // No prior non-built-in actions.
    indexInPanel = initialActionsInPanel.length;
  } else {
    // Prior non-built-in actions.  Find the index where the action goes.
    for (
      indexInPanel = sepIndex + 1;
      indexInPanel < initialActionsInPanel.length;
      indexInPanel++
    ) {
      let a = initialActionsInPanel[indexInPanel];
      if (a.getTitle().localeCompare(action.getTitle()) < 1) {
        break;
      }
    }
  }
  let expectedActionsInPanel = initialActionsInPanel.slice();
  expectedActionsInPanel.splice(indexInPanel, 0, action);
  // The separator between the built-ins and non-built-ins should be present
  // if it's not already.
  if (sepIndex < 0) {
    expectedActionsInPanel.splice(
      indexInPanel,
      0,
      new PageActions.Action({
        id: PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
        _isSeparator: true,
      })
    );
    sepIndex = indexInPanel;
    indexInPanel++;
  }
  Assert.deepEqual(
    PageActions.actionsInPanel(window),
    expectedActionsInPanel,
    "Actions in panel after adding the action"
  );

  Assert.deepEqual(
    PageActions.actionsInUrlbar(window),
    (gProtonUrlbar ? [action] : []).concat(initialActionsInUrlbar),
    "Actions in urlbar after adding the action"
  );

  // Check the set of all actions.
  Assert.deepEqual(
    new Set(PageActions.actions),
    new Set(initialActions.concat([action])),
    "All actions after adding the action"
  );

  Assert.deepEqual(
    PageActions.actionForID(action.id),
    action,
    "actionForID should be action"
  );

  Assert.ok(
    PageActions._persistedActions.ids.includes(action.id),
    "PageActions should record action in its list of seen actions"
  );

  // The action's panel button should have been created.
  let panelButtonNode =
    BrowserPageActions.mainViewBodyNode.children[indexInPanel];
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");
  Assert.equal(panelButtonNode.id, panelButtonID, "panelButtonID");
  Assert.equal(
    panelButtonNode.getAttribute("label"),
    action.getTitle(),
    "label"
  );

  // The separator between the built-ins and non-built-ins should exist.
  let sepNode = BrowserPageActions.mainViewBodyNode.children[sepIndex];
  Assert.notEqual(sepNode, null, "sepNode");
  Assert.equal(
    sepNode.id,
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    ),
    "sepNode.id"
  );

  let urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(!!urlbarButtonNode, gProtonUrlbar, "urlbarButtonNode");

  // Open the panel, click the action's button.
  await promiseOpenPageActionPanel();
  Assert.equal(
    onShowingInPanelCallCount,
    2,
    "onShowingInPanelCallCount should be inc'ed"
  );
  onCommandExpectedButtonID = panelButtonID;
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.equal(onCommandCallCount, 1, "onCommandCallCount should be inc'ed");

  // Show the action's button in the urlbar.
  action.pinnedToUrlbar = true;
  Assert.equal(
    onPlacedInUrlbarCallCount,
    1,
    "onPlacedInUrlbarCallCount should be inc'ed"
  );
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbarButtonNode");

  // The button should have been inserted before the bookmark star.
  Assert.notEqual(
    urlbarButtonNode.nextElementSibling,
    null,
    "Should be a next node"
  );
  Assert.equal(
    urlbarButtonNode.nextElementSibling.id,
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK).urlbarIDOverride,
    "Next node should be the bookmark star"
  );

  // Disable the action.  The button in the urlbar should be removed, and the
  // button in the panel should be disabled.
  action.setDisabled(true);
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbar button should be removed");
  Assert.equal(
    panelButtonNode.disabled,
    true,
    "panel button should be disabled"
  );

  // Enable the action.  The button in the urlbar should be added back, and the
  // button in the panel should be enabled.
  action.setDisabled(false);
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbar button should be added back");
  Assert.equal(
    panelButtonNode.disabled,
    false,
    "panel button should not be disabled"
  );

  // Click the urlbar button.
  onCommandExpectedButtonID = urlbarButtonID;
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  Assert.equal(onCommandCallCount, 2, "onCommandCallCount should be inc'ed");

  // Set a new title.
  let newTitle = title + " new title";
  action.setTitle(newTitle);
  Assert.equal(action.getTitle(), newTitle, "New title");
  Assert.equal(
    panelButtonNode.getAttribute("label"),
    action.getTitle(),
    "New label"
  );

  // Now that pinnedToUrlbar has been toggled, make sure that it sticks across
  // app restarts.  Simulate that by "unregistering" the action (not by removing
  // it, which is more permanent) and then registering it again.

  // unregister
  PageActions._actionsByID.delete(action.id);
  let index = PageActions._nonBuiltInActions.findIndex(a => a.id == action.id);
  Assert.ok(index >= 0, "Action should be in _nonBuiltInActions to begin with");
  PageActions._nonBuiltInActions.splice(index, 1);

  // register again
  PageActions._registerAction(action);

  // check relevant properties
  Assert.ok(
    PageActions._persistedActions.ids.includes(action.id),
    "PageActions should have 'seen' the action"
  );
  Assert.ok(
    PageActions._persistedActions.idsInUrlbar.includes(action.id),
    "idsInUrlbar should still include the action"
  );
  Assert.ok(action.pinnedToUrlbar, "pinnedToUrlbar should still be true");
  Assert.ok(
    action._pinnedToUrlbar,
    "_pinnedToUrlbar should still be true, for good measure"
  );

  // Remove the action.
  action.remove();
  panelButtonNode = document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode, null, "panelButtonNode");
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  let separatorNode = document.getElementById(
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    )
  );
  if (initialSepIndex < 0) {
    // The separator between the built-in actions and non-built-in actions
    // should be gone now, too.
    Assert.equal(separatorNode, null, "No separator");
    Assert.ok(
      !BrowserPageActions.mainViewBodyNode.lastElementChild.localName.includes(
        "separator"
      ),
      "Last child should not be separator"
    );
  } else {
    // The separator should still be present.
    Assert.notEqual(separatorNode, null, "Separator should still exist");
  }

  Assert.deepEqual(
    PageActions.actionsInPanel(window),
    initialActionsInPanel,
    "Actions in panel should go back to initial"
  );
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window),
    initialActionsInUrlbar,
    "Actions in urlbar should go back to initial"
  );
  Assert.deepEqual(
    PageActions.actions,
    initialActions,
    "Actions should go back to initial"
  );
  Assert.equal(
    PageActions.actionForID(action.id),
    null,
    "actionForID should be null"
  );

  Assert.ok(
    PageActions._persistedActions.ids.includes(action.id),
    "Action ID should remain in cache until purged"
  );
  PageActions._purgeUnregisteredPersistedActions();
  Assert.ok(
    !PageActions._persistedActions.ids.includes(action.id),
    "Action ID should be removed from cache after being purged"
  );
});

// Tests a non-built-in action with a subview.
add_task(async function withSubview() {
  let id = "test-subview";

  let onActionPlacedInPanelCallCount = 0;
  let onActionPlacedInUrlbarCallCount = 0;
  let onSubviewPlacedCount = 0;
  let onSubviewShowingCount = 0;

  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions.urlbarButtonNodeIDForActionID(id);

  let panelViewIDPanel = BrowserPageActions._panelViewNodeIDForActionID(
    id,
    false
  );
  let panelViewIDUrlbar = BrowserPageActions._panelViewNodeIDForActionID(
    id,
    true
  );

  let onSubviewPlacedExpectedPanelViewID = panelViewIDPanel;
  let onSubviewShowingExpectedPanelViewID;

  let action = PageActions.addAction(
    new PageActions.Action({
      iconURL: "chrome://browser/skin/mail.svg",
      id,
      pinnedToUrlbar: true,
      title: "Test subview",
      wantsSubview: true,
      onPlacedInPanel(buttonNode) {
        onActionPlacedInPanelCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, panelButtonID, "buttonNode.id");
      },
      onPlacedInUrlbar(buttonNode) {
        onActionPlacedInUrlbarCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, urlbarButtonID, "buttonNode.id");
      },
      onSubviewPlaced(panelViewNode) {
        onSubviewPlacedCount++;
        Assert.ok(
          panelViewNode,
          "panelViewNode should be non-null: " + panelViewNode
        );
        Assert.equal(
          panelViewNode.id,
          onSubviewPlacedExpectedPanelViewID,
          "panelViewNode.id"
        );
      },
      onSubviewShowing(panelViewNode) {
        onSubviewShowingCount++;
        Assert.ok(
          panelViewNode,
          "panelViewNode should be non-null: " + panelViewNode
        );
        Assert.equal(
          panelViewNode.id,
          onSubviewShowingExpectedPanelViewID,
          "panelViewNode.id"
        );
      },
    })
  );

  Assert.equal(action.id, id, "id");
  Assert.equal(action.getWantsSubview(), true, "subview");
  Assert.equal(action.getWantsSubview(window), true, "subview in window");

  // The action shouldn't be placed in the panel until it opens for the first
  // time.
  Assert.equal(
    onActionPlacedInPanelCallCount,
    0,
    "onActionPlacedInPanelCallCount should be 0"
  );
  Assert.equal(onSubviewPlacedCount, 0, "onSubviewPlacedCount should be 0");

  // But it should be placed in the urlbar.
  Assert.equal(
    onActionPlacedInUrlbarCallCount,
    1,
    "onActionPlacedInUrlbarCallCount should be 0"
  );

  // Open the panel, which should place the action in it.
  await promiseOpenPageActionPanel();

  Assert.equal(
    onActionPlacedInPanelCallCount,
    1,
    "onActionPlacedInPanelCallCount should be inc'ed"
  );
  Assert.equal(
    onSubviewPlacedCount,
    1,
    "onSubviewPlacedCount should be inc'ed"
  );
  Assert.equal(
    onSubviewShowingCount,
    0,
    "onSubviewShowingCount should remain 0"
  );

  // The action's panel button and view (in the main page action panel) should
  // have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");

  // The action's urlbar button should have been created.
  let urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbarButtonNode");

  // The button should have been inserted before the bookmark star.
  Assert.notEqual(
    urlbarButtonNode.nextElementSibling,
    null,
    "Should be a next node"
  );
  Assert.equal(
    urlbarButtonNode.nextElementSibling.id,
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK).urlbarIDOverride,
    "Next node should be the bookmark star"
  );

  // Click the action's button in the panel.  The subview should be shown.
  Assert.equal(
    onSubviewShowingCount,
    0,
    "onSubviewShowingCount should remain 0"
  );
  let subviewShownPromise = promisePageActionViewShown();
  onSubviewShowingExpectedPanelViewID = panelViewIDPanel;
  panelButtonNode.click();
  await subviewShownPromise;

  // Click the main button to hide the main panel.
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  // Click the action's urlbar button, which should open the activated-action
  // panel showing the subview.
  onSubviewPlacedExpectedPanelViewID = panelViewIDUrlbar;
  onSubviewShowingExpectedPanelViewID = panelViewIDUrlbar;
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(
    onSubviewPlacedCount,
    2,
    "onSubviewPlacedCount should be inc'ed"
  );
  Assert.equal(
    onSubviewShowingCount,
    2,
    "onSubviewShowingCount should be inc'ed"
  );

  // Click the urlbar button again.  The activated-action panel should close.
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  assertActivatedPageActionPanelHidden();

  // Remove the action.
  action.remove();
  panelButtonNode = document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode, null, "panelButtonNode");
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");
  let panelViewNodePanel = document.getElementById(panelViewIDPanel);
  Assert.equal(panelViewNodePanel, null, "panelViewNodePanel");
  let panelViewNodeUrlbar = document.getElementById(panelViewIDUrlbar);
  Assert.equal(panelViewNodeUrlbar, null, "panelViewNodeUrlbar");
});

// Tests a non-built-in action with an iframe.
add_task(async function withIframe() {
  let id = "test-iframe";

  let onCommandCallCount = 0;
  let onPlacedInPanelCallCount = 0;
  let onPlacedInUrlbarCallCount = 0;
  let onIframeShowingCount = 0;

  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions.urlbarButtonNodeIDForActionID(id);

  let action = PageActions.addAction(
    new PageActions.Action({
      iconURL: "chrome://browser/skin/mail.svg",
      id,
      pinnedToUrlbar: true,
      title: "Test iframe",
      wantsIframe: true,
      onCommand(event, buttonNode) {
        onCommandCallCount++;
      },
      onIframeShowing(iframeNode, panelNode) {
        onIframeShowingCount++;
        Assert.ok(iframeNode, "iframeNode should be non-null: " + iframeNode);
        Assert.equal(iframeNode.localName, "iframe", "iframe localName");
        Assert.ok(panelNode, "panelNode should be non-null: " + panelNode);
        Assert.equal(
          panelNode.id,
          BrowserPageActions._activatedActionPanelID,
          "panelNode.id"
        );
      },
      onPlacedInPanel(buttonNode) {
        onPlacedInPanelCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, panelButtonID, "buttonNode.id");
      },
      onPlacedInUrlbar(buttonNode) {
        onPlacedInUrlbarCallCount++;
        Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
        Assert.equal(buttonNode.id, urlbarButtonID, "buttonNode.id");
      },
    })
  );

  Assert.equal(action.id, id, "id");
  Assert.equal(action.wantsIframe, true, "wantsIframe");

  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  Assert.equal(
    onPlacedInPanelCallCount,
    1,
    "onPlacedInPanelCallCount should be inc'ed"
  );
  Assert.equal(
    onPlacedInUrlbarCallCount,
    1,
    "onPlacedInUrlbarCallCount should be inc'ed"
  );
  Assert.equal(onIframeShowingCount, 0, "onIframeShowingCount should remain 0");
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");

  // The action's panel button should have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");

  // The action's urlbar button should have been created.
  let urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbarButtonNode");

  // The button should have been inserted before the bookmark star.
  Assert.notEqual(
    urlbarButtonNode.nextElementSibling,
    null,
    "Should be a next node"
  );
  Assert.equal(
    urlbarButtonNode.nextElementSibling.id,
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK).urlbarIDOverride,
    "Next node should be the bookmark star"
  );

  // Open the panel, click the action's button.
  await promiseOpenPageActionPanel();
  Assert.equal(onIframeShowingCount, 0, "onIframeShowingCount should remain 0");
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onCommandCallCount, 1, "onCommandCallCount should be inc'ed");
  Assert.equal(
    onIframeShowingCount,
    1,
    "onIframeShowingCount should be inc'ed"
  );

  // The activated-action panel should have opened, anchored to the action's
  // urlbar button.
  let aaPanel = document.getElementById(
    BrowserPageActions._activatedActionPanelID
  );
  Assert.notEqual(aaPanel, null, "activated-action panel");
  Assert.equal(aaPanel.anchorNode.id, urlbarButtonID, "aaPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  assertActivatedPageActionPanelHidden();

  // Click the action's urlbar button.
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onCommandCallCount, 2, "onCommandCallCount should be inc'ed");
  Assert.equal(
    onIframeShowingCount,
    2,
    "onIframeShowingCount should be inc'ed"
  );

  // The activated-action panel should have opened, again anchored to the
  // action's urlbar button.
  aaPanel = document.getElementById(BrowserPageActions._activatedActionPanelID);
  Assert.notEqual(aaPanel, null, "aaPanel");
  Assert.equal(aaPanel.anchorNode.id, urlbarButtonID, "aaPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  assertActivatedPageActionPanelHidden();

  // Hide the action's button in the urlbar.
  action.pinnedToUrlbar = false;
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  // Open the panel, click the action's button.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onCommandCallCount, 3, "onCommandCallCount should be inc'ed");
  Assert.equal(
    onIframeShowingCount,
    3,
    "onIframeShowingCount should be inc'ed"
  );

  // The activated-action panel should have opened, this time anchored to the
  // main page action button in the urlbar.
  aaPanel = document.getElementById(BrowserPageActions._activatedActionPanelID);
  Assert.notEqual(aaPanel, null, "aaPanel");
  Assert.equal(
    aaPanel.anchorNode.id,
    BrowserPageActions.mainButtonNode.id,
    "aaPanel.anchorNode.id"
  );
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  assertActivatedPageActionPanelHidden();

  // Remove the action.
  action.remove();
  panelButtonNode = document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode, null, "panelButtonNode");
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");
});

// Tests an action with the _insertBeforeActionID option set.
add_task(async function insertBeforeActionID() {
  let id = "test-insertBeforeActionID";
  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);

  let initialActions = PageActions.actionsInPanel(window);
  let initialBuiltInActions = PageActions._builtInActions.slice();
  let initialNonBuiltInActions = PageActions._nonBuiltInActions.slice();
  let initialBookmarkSeparatorIndex = initialActions.findIndex(a => {
    return a.id == PageActions.ACTION_ID_BOOKMARK_SEPARATOR;
  });

  let action = PageActions.addAction(
    new PageActions.Action({
      id,
      title: "Test insertBeforeActionID",
      _insertBeforeActionID: PageActions.ACTION_ID_BOOKMARK_SEPARATOR,
    })
  );

  Assert.equal(action.id, id, "id");
  Assert.ok("__insertBeforeActionID" in action, "__insertBeforeActionID");
  Assert.equal(
    action.__insertBeforeActionID,
    PageActions.ACTION_ID_BOOKMARK_SEPARATOR,
    "action.__insertBeforeActionID"
  );

  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  let newActions = PageActions.actionsInPanel(window);
  Assert.equal(
    newActions.length,
    initialActions.length + 1,
    "PageActions.actions.length should be updated"
  );
  Assert.equal(
    PageActions._builtInActions.length,
    initialBuiltInActions.length + 1,
    "PageActions._builtInActions.length should be updated"
  );
  Assert.equal(
    PageActions._nonBuiltInActions.length,
    initialNonBuiltInActions.length,
    "PageActions._nonBuiltInActions.length should remain the same"
  );

  // The action's panel button should have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");

  if (!gProtonUrlbar) {
    let actionIndex = newActions.findIndex(a => a.id == id);
    Assert.equal(
      initialBookmarkSeparatorIndex,
      actionIndex,
      "initialBookmarkSeparatorIndex"
    );
    let newBookmarkSeparatorIndex = newActions.findIndex(a => {
      return a.id == PageActions.ACTION_ID_BOOKMARK_SEPARATOR;
    });
    Assert.equal(
      newBookmarkSeparatorIndex,
      initialBookmarkSeparatorIndex + 1,
      "newBookmarkSeparatorIndex"
    );

    // The button's next sibling should be the bookmark separator.
    Assert.notEqual(
      panelButtonNode.nextElementSibling,
      null,
      "panelButtonNode.nextElementSibling"
    );
    Assert.equal(
      panelButtonNode.nextElementSibling.id,
      BrowserPageActions.panelButtonNodeIDForActionID(
        PageActions.ACTION_ID_BOOKMARK_SEPARATOR
      ),
      "panelButtonNode.nextElementSibling.id"
    );
  }

  // The separator between the built-in and non-built-in actions should not have
  // been created.
  Assert.equal(
    document.getElementById(
      BrowserPageActions.panelButtonNodeIDForActionID(
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR
      )
    ),
    null,
    "Separator should be gone"
  );

  action.remove();
});

// Tests that the ordering in the panel of multiple non-built-in actions is
// alphabetical.
add_task(async function multipleNonBuiltInOrdering() {
  let idPrefix = "test-multipleNonBuiltInOrdering-";
  let titlePrefix = "Test multipleNonBuiltInOrdering ";

  let initialActions = PageActions.actionsInPanel(window);
  let initialBuiltInActions = PageActions._builtInActions.slice();
  let initialNonBuiltInActions = PageActions._nonBuiltInActions.slice();

  // Create some actions in an out-of-order order.
  let actions = [2, 1, 4, 3].map(index => {
    return PageActions.addAction(
      new PageActions.Action({
        id: idPrefix + index,
        title: titlePrefix + index,
      })
    );
  });

  // + 1 for the separator between built-in and non-built-in actions.
  Assert.equal(
    PageActions.actionsInPanel(window).length,
    initialActions.length + actions.length + 1,
    "PageActions.actionsInPanel().length should be updated"
  );

  Assert.equal(
    PageActions._builtInActions.length,
    initialBuiltInActions.length,
    "PageActions._builtInActions.length should be same"
  );
  Assert.equal(
    PageActions._nonBuiltInActions.length,
    initialNonBuiltInActions.length + actions.length,
    "PageActions._nonBuiltInActions.length should be updated"
  );

  // Look at the final actions.length actions in PageActions.actions, from first
  // to last.
  for (let i = 0; i < actions.length; i++) {
    let expectedIndex = i + 1;
    let actualAction = PageActions._nonBuiltInActions[i];
    Assert.equal(
      actualAction.id,
      idPrefix + expectedIndex,
      "actualAction.id for index: " + i
    );
  }

  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();

  // Check the button nodes in the panel.
  let expectedIndex = 1;
  let buttonNode = document.getElementById(
    BrowserPageActions.panelButtonNodeIDForActionID(idPrefix + expectedIndex)
  );
  Assert.notEqual(buttonNode, null, "buttonNode");
  Assert.notEqual(
    buttonNode.previousElementSibling,
    null,
    "buttonNode.previousElementSibling"
  );
  Assert.equal(
    buttonNode.previousElementSibling.id,
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    ),
    "buttonNode.previousElementSibling.id"
  );
  for (let i = 0; i < actions.length; i++) {
    Assert.notEqual(buttonNode, null, "buttonNode at index: " + i);
    Assert.equal(
      buttonNode.id,
      BrowserPageActions.panelButtonNodeIDForActionID(idPrefix + expectedIndex),
      "buttonNode.id at index: " + i
    );
    buttonNode = buttonNode.nextElementSibling;
    expectedIndex++;
  }
  Assert.equal(buttonNode, null, "Nothing should come after the last button");

  for (let action of actions) {
    action.remove();
  }

  // The separator between the built-in and non-built-in actions should be gone.
  Assert.equal(
    document.getElementById(
      BrowserPageActions.panelButtonNodeIDForActionID(
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR
      )
    ),
    null,
    "Separator should be gone"
  );
});

// Makes sure the panel is correctly updated when a non-built-in action is
// added before the built-in actions; and when all built-in actions are removed
// and added back.
add_task(async function nonBuiltFirst() {
  let initialActions = PageActions.actions;
  let initialActionsInPanel = PageActions.actionsInPanel(window);

  // Remove all actions.
  for (let action of initialActions) {
    action.remove();
  }

  // Check the actions.
  Assert.deepEqual(
    PageActions.actions.map(a => a.id),
    [],
    "PageActions.actions should be empty"
  );
  Assert.deepEqual(
    PageActions._builtInActions.map(a => a.id),
    [],
    "PageActions._builtInActions should be empty"
  );
  Assert.deepEqual(
    PageActions._nonBuiltInActions.map(a => a.id),
    [],
    "PageActions._nonBuiltInActions should be empty"
  );

  // Check the panel.
  Assert.equal(
    BrowserPageActions.mainViewBodyNode.children.length,
    0,
    "All nodes should be gone"
  );

  // Add a non-built-in action.
  let action = PageActions.addAction(
    new PageActions.Action({
      id: "test-nonBuiltFirst",
      title: "Test nonBuiltFirst",
    })
  );

  // Check the actions.
  Assert.deepEqual(
    PageActions.actions.map(a => a.id),
    [action.id],
    "Action should be in PageActions.actions"
  );
  Assert.deepEqual(
    PageActions._builtInActions.map(a => a.id),
    [],
    "PageActions._builtInActions should be empty"
  );
  Assert.deepEqual(
    PageActions._nonBuiltInActions.map(a => a.id),
    [action.id],
    "Action should be in PageActions._nonBuiltInActions"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    [BrowserPageActions.panelButtonNodeIDForActionID(action.id)],
    "Action should be in panel"
  );

  // Now add back all the actions.
  for (let a of initialActions) {
    PageActions.addAction(a);
  }

  // Check the actions.
  Assert.deepEqual(
    new Set(PageActions.actions.map(a => a.id)),
    new Set(initialActions.map(a => a.id).concat([action.id])),
    "All actions should be in PageActions.actions"
  );
  Assert.deepEqual(
    PageActions._builtInActions.map(a => a.id),
    initialActions.filter(a => !a.__transient).map(a => a.id),
    "PageActions._builtInActions should be initial actions"
  );
  Assert.deepEqual(
    PageActions._nonBuiltInActions.map(a => a.id),
    [action.id],
    "PageActions._nonBuiltInActions should contain action"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_BUILT_IN_SEPARATOR], [action.id]),
    "All actions should be in PageActions.actionsInPanel()"
  );
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_BUILT_IN_SEPARATOR], [action.id])
      .map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
    "Panel should contain all actions"
  );

  // Remove the test action.
  action.remove();

  // Check the actions.
  Assert.deepEqual(
    PageActions.actions.map(a => a.id),
    initialActions.map(a => a.id),
    "Action should no longer be in PageActions.actions"
  );
  Assert.deepEqual(
    PageActions._builtInActions.map(a => a.id),
    initialActions.filter(a => !a.__transient).map(a => a.id),
    "PageActions._builtInActions should be initial actions"
  );
  Assert.deepEqual(
    PageActions._nonBuiltInActions.map(a => a.id),
    [],
    "Action should no longer be in PageActions._nonBuiltInActions"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel.map(a => a.id),
    "Action should no longer be in PageActions.actionsInPanel()"
  );
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel.map(a =>
      BrowserPageActions.panelButtonNodeIDForActionID(a.id)
    ),
    "Action should no longer be in panel"
  );
});

// Adds an action, changes its placement in the urlbar to something non-default,
// removes the action, and then adds it back.  Since the action was removed and
// re-added without restarting the app (or more accurately without calling
// PageActions._purgeUnregisteredPersistedActions), the action should remain in
// persisted state and retain its last placement in the urlbar.
add_task(async function removeRetainState() {
  // Get the list of actions initially in the urlbar.
  let initialActionsInUrlbar = PageActions.actionsInUrlbar(window);
  Assert.ok(
    !!initialActionsInUrlbar.length,
    "This test expects there to be at least one action in the urlbar initially (like the bookmark star)"
  );

  // Add a test action.
  let id = "test-removeRetainState";
  let testAction = PageActions.addAction(
    new PageActions.Action({
      id,
      title: "Test removeRetainState",
    })
  );

  // Show its button in the urlbar.
  testAction.pinnedToUrlbar = true;

  // "Move" the test action to the front of the urlbar by toggling
  // pinnedToUrlbar for all the other actions in the urlbar.
  for (let action of initialActionsInUrlbar) {
    action.pinnedToUrlbar = false;
    action.pinnedToUrlbar = true;
  }

  // Check the actions in PageActions.actionsInUrlbar.
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window).map(a => a.id),
    [testAction].concat(initialActionsInUrlbar).map(a => a.id),
    "PageActions.actionsInUrlbar should be in expected order: testAction followed by all initial actions"
  );

  // Check the nodes in the urlbar.
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
    [testAction]
      .concat(initialActionsInUrlbar)
      .map(a => BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)),
    "urlbar nodes should be in expected order: testAction followed by all initial actions"
  );

  // Remove the test action.
  testAction.remove();

  // Check the actions in PageActions.actionsInUrlbar.
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window).map(a => a.id),
    initialActionsInUrlbar.map(a => a.id),
    "PageActions.actionsInUrlbar should be in expected order after removing test action: all initial actions"
  );

  // Check the nodes in the urlbar.
  actualUrlbarNodeIDs = [];
  for (
    let node = BrowserPageActions.mainButtonNode.nextElementSibling;
    node;
    node = node.nextElementSibling
  ) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    initialActionsInUrlbar.map(a =>
      BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)
    ),
    "urlbar nodes should be in expected order after removing test action: all initial actions"
  );

  // Add the test action again.
  testAction = PageActions.addAction(
    new PageActions.Action({
      id,
      title: "Test removeRetainState",
    })
  );

  // Show its button in the urlbar again.
  testAction.pinnedToUrlbar = true;

  // Check the actions in PageActions.actionsInUrlbar.
  Assert.deepEqual(
    PageActions.actionsInUrlbar(window).map(a => a.id),
    [testAction].concat(initialActionsInUrlbar).map(a => a.id),
    "PageActions.actionsInUrlbar should be in expected order after re-adding test action: testAction followed by all initial actions"
  );

  // Check the nodes in the urlbar.
  actualUrlbarNodeIDs = [];
  for (
    let node = BrowserPageActions.mainButtonNode.nextElementSibling;
    node;
    node = node.nextElementSibling
  ) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    [testAction]
      .concat(initialActionsInUrlbar)
      .map(a => BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)),
    "urlbar nodes should be in expected order after re-adding test action: testAction followed by all initial actions"
  );

  // Done, clean up.
  testAction.remove();
});

// Tests transient actions.
add_task(async function transient() {
  let initialActionsInPanel = PageActions.actionsInPanel(window);

  let onPlacedInPanelCount = 0;
  let onBeforePlacedInWindowCount = 0;

  let action = PageActions.addAction(
    new PageActions.Action({
      id: "test-transient",
      title: "Test transient",
      _transient: true,
      onPlacedInPanel(buttonNode) {
        onPlacedInPanelCount++;
      },
      onBeforePlacedInWindow(win) {
        onBeforePlacedInWindowCount++;
      },
    })
  );

  Assert.equal(action.__transient, true, "__transient");

  Assert.equal(onPlacedInPanelCount, 0, "onPlacedInPanelCount should remain 0");
  Assert.equal(
    onBeforePlacedInWindowCount,
    gProtonUrlbar ? 1 : 0,
    "onBeforePlacedInWindowCount after adding transient action"
  );

  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_TRANSIENT_SEPARATOR, action.id]),
    "PageActions.actionsInPanel() should be updated"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_TRANSIENT_SEPARATOR, action.id])
      .map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
    "Actions in panel should be correct"
  );

  Assert.equal(
    onPlacedInPanelCount,
    1,
    "onPlacedInPanelCount should be inc'ed"
  );
  Assert.equal(
    onBeforePlacedInWindowCount,
    1,
    "onBeforePlacedInWindowCount should be inc'ed"
  );

  // Disable the action.  It should be removed from the panel.
  action.setDisabled(true, window);

  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel.map(a => a.id),
    "PageActions.actionsInPanel() should revert to initial"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel.map(a =>
      BrowserPageActions.panelButtonNodeIDForActionID(a.id)
    ),
    "Actions in panel should be correct"
  );

  // Enable the action.  It should be added back to the panel.
  action.setDisabled(false, window);

  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_TRANSIENT_SEPARATOR, action.id]),
    "PageActions.actionsInPanel() should be updated"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_TRANSIENT_SEPARATOR, action.id])
      .map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
    "Actions in panel should be correct"
  );

  Assert.equal(
    onPlacedInPanelCount,
    2,
    "onPlacedInPanelCount should be inc'ed"
  );
  Assert.equal(
    onBeforePlacedInWindowCount,
    2,
    "onBeforePlacedInWindowCount should be inc'ed"
  );

  // Add another non-built in but non-transient action.
  let otherAction = PageActions.addAction(
    new PageActions.Action({
      id: "test-transient2",
      title: "Test transient 2",
    })
  );

  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
        otherAction.id,
        PageActions.ACTION_ID_TRANSIENT_SEPARATOR,
        action.id,
      ]),
    "PageActions.actionsInPanel() should be updated"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
        otherAction.id,
        PageActions.ACTION_ID_TRANSIENT_SEPARATOR,
        action.id,
      ])
      .map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
    "Actions in panel should be correct"
  );

  Assert.equal(
    onPlacedInPanelCount,
    2,
    "onPlacedInPanelCount should remain the same"
  );
  Assert.equal(
    onBeforePlacedInWindowCount,
    2,
    "onBeforePlacedInWindowCount should remain the same"
  );

  // Disable the action again.  It should be removed from the panel.
  action.setDisabled(true, window);

  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_BUILT_IN_SEPARATOR, otherAction.id]),
    "PageActions.actionsInPanel() should be updated"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([PageActions.ACTION_ID_BUILT_IN_SEPARATOR, otherAction.id])
      .map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
    "Actions in panel should be correct"
  );

  // Enable the action again.  It should be added back to the panel.
  action.setDisabled(false, window);

  Assert.deepEqual(
    PageActions.actionsInPanel(window).map(a => a.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
        otherAction.id,
        PageActions.ACTION_ID_TRANSIENT_SEPARATOR,
        action.id,
      ]),
    "PageActions.actionsInPanel() should be updated"
  );

  // Check the panel.
  await promiseOpenPageActionPanel();
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.deepEqual(
    Array.from(BrowserPageActions.mainViewBodyNode.children, n => n.id),
    initialActionsInPanel
      .map(a => a.id)
      .concat([
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
        otherAction.id,
        PageActions.ACTION_ID_TRANSIENT_SEPARATOR,
        action.id,
      ])
      .map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
    "Actions in panel should be correct"
  );

  Assert.equal(
    onPlacedInPanelCount,
    3,
    "onPlacedInPanelCount should be inc'ed"
  );
  Assert.equal(
    onBeforePlacedInWindowCount,
    3,
    "onBeforePlacedInWindowCount should be inc'ed"
  );

  // Done, clean up.
  action.remove();
  otherAction.remove();
});
