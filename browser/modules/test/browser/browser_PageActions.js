/* eslint-disable mozilla/no-arbitrary-setTimeout */
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

  await disableNonReleaseActions();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.removeTab(tab);
  });
});


// Tests a simple non-built-in action without an iframe or subview.  Also
// thoroughly checks most of the action's properties, methods, and DOM nodes, so
// it's not necessary to do that in general in other test tasks.
add_task(async function simple() {
  let iconURL = "chrome://browser/skin/mail.svg";
  let id = "test-simple";
  let nodeAttributes = {
    "test-attr": "test attr value",
  };
  let title = "Test simple";
  let tooltip = "Test simple tooltip";

  let onCommandCallCount = 0;
  let onPlacedInPanelCallCount = 0;
  let onPlacedInUrlbarCallCount = 0;
  let onShowingInPanelCallCount = 0;
  let onCommandExpectedButtonID;

  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions.urlbarButtonNodeIDForActionID(id);

  let initialActions = PageActions.actions;

  let action = PageActions.addAction(new PageActions.Action({
    iconURL,
    id,
    nodeAttributes,
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
  }));

  Assert.equal(action.getIconURL(), iconURL, "iconURL");
  Assert.equal(action.id, id, "id");
  Assert.deepEqual(action.nodeAttributes, nodeAttributes, "nodeAttributes");
  Assert.equal(action.shownInUrlbar, false, "shownInUrlbar");
  Assert.equal(action.subview, null, "subview");
  Assert.equal(action.getTitle(), title, "title");
  Assert.equal(action.getTooltip(), tooltip, "tooltip");
  Assert.equal(action.urlbarIDOverride, null, "urlbarIDOverride");
  Assert.equal(action.wantsIframe, false, "wantsIframe");

  Assert.ok(!("__insertBeforeActionID" in action), "__insertBeforeActionID");
  Assert.ok(!("__isSeparator" in action), "__isSeparator");
  Assert.ok(!("__urlbarNodeInMarkup" in action), "__urlbarNodeInMarkup");

  Assert.equal(onPlacedInPanelCallCount, 1,
               "onPlacedInPanelCallCount should be inc'ed");
  Assert.equal(onPlacedInUrlbarCallCount, 0,
               "onPlacedInUrlbarCallCount should remain 0");
  Assert.equal(onShowingInPanelCallCount, 0,
               "onShowingInPanelCallCount should remain 0");

  // The separator between the built-in and non-built-in actions should have
  // been created and included in PageActions.actions, which is why the new
  // count should be the initial count + 2, not + 1.
  Assert.equal(PageActions.actions.length, initialActions.length + 2,
               "PageActions.actions.length should be updated");
  Assert.deepEqual(PageActions.actions[PageActions.actions.length - 1], action,
                   "Last page action should be action");
  Assert.equal(PageActions.actions[PageActions.actions.length - 2].id,
               PageActions.ACTION_ID_BUILT_IN_SEPARATOR,
               "2nd-to-last page action should be separator");

  Assert.deepEqual(PageActions.actionForID(action.id), action,
                   "actionForID should be action");

  Assert.ok(PageActions._persistedActions.ids.includes(action.id),
            "PageActions should record action in its list of seen actions");

  // The action's panel button should have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");
  Assert.equal(panelButtonNode.getAttribute("label"), action.getTitle(),
               "label");
  for (let name in action.nodeAttributes) {
    Assert.ok(panelButtonNode.hasAttribute(name), "Has attribute: " + name);
    Assert.equal(panelButtonNode.getAttribute(name),
                 action.nodeAttributes[name],
                 "Equal attribute: " + name);
  }

  // The panel button should be the last node in the panel, and its previous
  // sibling should be the separator between the built-in actions and non-built-
  // in actions.
  Assert.equal(panelButtonNode.nextSibling, null, "nextSibling");
  Assert.notEqual(panelButtonNode.previousSibling, null, "previousSibling");
  Assert.equal(
    panelButtonNode.previousSibling.id,
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    ),
    "previousSibling.id"
  );

  // The action's urlbar button should not have been created.
  let urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  // Open the panel, click the action's button.
  await promisePageActionPanelOpen();
  Assert.equal(onShowingInPanelCallCount, 1,
               "onShowingInPanelCallCount should be inc'ed");
  onCommandExpectedButtonID = panelButtonID;
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePageActionPanelHidden();
  Assert.equal(onCommandCallCount, 1, "onCommandCallCount should be inc'ed");

  // Show the action's button in the urlbar.
  action.shownInUrlbar = true;
  Assert.equal(onPlacedInUrlbarCallCount, 1,
               "onPlacedInUrlbarCallCount should be inc'ed");
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbarButtonNode");
  for (let name in action.nodeAttributes) {
    Assert.ok(urlbarButtonNode.hasAttribute(name), name,
              "Has attribute: " + name);
    Assert.equal(urlbarButtonNode.getAttribute(name),
                 action.nodeAttributes[name],
                 "Equal attribute: " + name);
  }

  // The button should have been inserted before the bookmark star.
  Assert.notEqual(urlbarButtonNode.nextSibling, null, "Should be a next node");
  Assert.equal(
    urlbarButtonNode.nextSibling.id,
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK).urlbarIDOverride,
    "Next node should be the bookmark star"
  );

  // Click the urlbar button.
  onCommandExpectedButtonID = urlbarButtonID;
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  Assert.equal(onCommandCallCount, 2, "onCommandCallCount should be inc'ed");

  // Set a new title.
  let newTitle = title + " new title";
  action.setTitle(newTitle);
  Assert.equal(action.getTitle(), newTitle, "New title");
  Assert.equal(panelButtonNode.getAttribute("label"), action.getTitle(),
               "New label");

  // Now that shownInUrlbar has been toggled, make sure that it sticks across
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
  Assert.ok(PageActions._persistedActions.ids.includes(action.id),
            "PageActions should have 'seen' the action");
  Assert.ok(PageActions._persistedActions.idsInUrlbar.includes(action.id),
            "idsInUrlbar should still include the action");
  Assert.ok(action.shownInUrlbar,
            "shownInUrlbar should still be true");
  Assert.ok(action._shownInUrlbar,
            "_shownInUrlbar should still be true, for good measure");

  // Remove the action.
  action.remove();
  panelButtonNode = document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode, null, "panelButtonNode");
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  // The separator between the built-in actions and non-built-in actions should
  // be gone now, too.
  let separatorNode = document.getElementById(
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    )
  );
  Assert.equal(separatorNode, null, "No separator");
  Assert.ok(!BrowserPageActions.mainViewBodyNode
            .lastChild.localName.includes("separator"),
            "Last child should not be separator");

  Assert.deepEqual(PageActions.actions, initialActions,
                   "Actions should go back to initial");
  Assert.equal(PageActions.actionForID(action.id), null,
               "actionForID should be null");

  Assert.ok(PageActions._persistedActions.ids.includes(action.id),
            "Action ID should remain in cache until purged");
  PageActions._purgeUnregisteredPersistedActions();
  Assert.ok(!PageActions._persistedActions.ids.includes(action.id),
            "Action ID should be removed from cache after being purged");
});


// Tests a non-built-in action with a subview.
add_task(async function withSubview() {
  let id = "test-subview";

  let onActionCommandCallCount = 0;
  let onActionPlacedInPanelCallCount = 0;
  let onActionPlacedInUrlbarCallCount = 0;
  let onSubviewPlacedCount = 0;
  let onSubviewShowingCount = 0;
  let onButtonCommandCallCount = 0;

  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions.urlbarButtonNodeIDForActionID(id);

  let panelViewIDPanel =
    BrowserPageActions._panelViewNodeIDForActionID(id, false);
  let panelViewIDUrlbar =
    BrowserPageActions._panelViewNodeIDForActionID(id, true);

  let onSubviewPlacedExpectedPanelViewID = panelViewIDPanel;
  let onSubviewShowingExpectedPanelViewID;
  let onButtonCommandExpectedButtonID;

  let subview = {
    buttons: [0, 1, 2].map(index => {
      return {
        id: "test-subview-button-" + index,
        title: "Test subview Button " + index,
      };
    }),
    onPlaced(panelViewNode) {
      onSubviewPlacedCount++;
      Assert.ok(panelViewNode,
                "panelViewNode should be non-null: " + panelViewNode);
      Assert.equal(panelViewNode.id, onSubviewPlacedExpectedPanelViewID,
                   "panelViewNode.id");
    },
    onShowing(panelViewNode) {
      onSubviewShowingCount++;
      Assert.ok(panelViewNode,
                "panelViewNode should be non-null: " + panelViewNode);
      Assert.equal(panelViewNode.id, onSubviewShowingExpectedPanelViewID,
                   "panelViewNode.id");
    },
  };
  subview.buttons[0].onCommand = (event, buttonNode) => {
    onButtonCommandCallCount++;
    Assert.ok(event, "event should be non-null: " + event);
    Assert.ok(buttonNode, "buttonNode should be non-null: " + buttonNode);
    Assert.equal(buttonNode.id, onButtonCommandExpectedButtonID,
                 "buttonNode.id");
    for (let node = buttonNode.parentNode; node; node = node.parentNode) {
      if (node.localName == "panel") {
        node.hidePopup();
        break;
      }
    }
  };

  let action = PageActions.addAction(new PageActions.Action({
    iconURL: "chrome://browser/skin/mail.svg",
    id,
    shownInUrlbar: true,
    subview,
    title: "Test subview",
    onCommand(event, buttonNode) {
      onActionCommandCallCount++;
    },
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
  }));

  let panelViewButtonIDPanel =
    BrowserPageActions._panelViewButtonNodeIDForActionID(
      id, subview.buttons[0].id, false
    );
  let panelViewButtonIDUrlbar =
    BrowserPageActions._panelViewButtonNodeIDForActionID(
      id, subview.buttons[0].id, true
    );

  Assert.equal(action.id, id, "id");
  Assert.notEqual(action.subview, null, "subview");
  Assert.notEqual(action.subview.buttons, null, "subview.buttons");
  Assert.equal(action.subview.buttons.length, subview.buttons.length,
               "subview.buttons.length");
  for (let i = 0; i < subview.buttons.length; i++) {
    Assert.equal(action.subview.buttons[i].id, subview.buttons[i].id,
                 "subview button id for index: " + i);
    Assert.equal(action.subview.buttons[i].title, subview.buttons[i].title,
                 "subview button title for index: " + i);
  }

  Assert.equal(onActionPlacedInPanelCallCount, 1,
               "onActionPlacedInPanelCallCount should be inc'ed");
  Assert.equal(onActionPlacedInUrlbarCallCount, 1,
               "onActionPlacedInUrlbarCallCount should be inc'ed");
  Assert.equal(onSubviewPlacedCount, 1,
               "onSubviewPlacedCount should be inc'ed");
  Assert.equal(onSubviewShowingCount, 0,
               "onSubviewShowingCount should remain 0");

  // The action's panel button and view (in the main page action panel) should
  // have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");
  let panelViewButtonNodePanel =
    document.getElementById(panelViewButtonIDPanel);
  Assert.notEqual(panelViewButtonNodePanel, null, "panelViewButtonNodePanel");

  // The action's urlbar button should have been created.
  let urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbarButtonNode");

  // The button should have been inserted before the bookmark star.
  Assert.notEqual(urlbarButtonNode.nextSibling, null, "Should be a next node");
  Assert.equal(
    urlbarButtonNode.nextSibling.id,
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK).urlbarIDOverride,
    "Next node should be the bookmark star"
  );

  // Open the panel, click the action's button, click the subview's first
  // button.
  await promisePageActionPanelOpen();
  Assert.equal(onSubviewShowingCount, 0,
               "onSubviewShowingCount should remain 0");
  let subviewShownPromise = promisePageActionViewShown();
  onSubviewShowingExpectedPanelViewID = panelViewIDPanel;

  // synthesizeMouse often cannot seem to click the right node when used on
  // buttons that show subviews and buttons inside subviews.  That's why we're
  // using node.click() twice here: the first time to show the subview, the
  // second time to click a button in the subview.
//   EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  panelButtonNode.click();
  await subviewShownPromise;
  Assert.equal(onActionCommandCallCount, 0,
               "onActionCommandCallCount should remain 0");
  Assert.equal(onSubviewShowingCount, 1,
               "onSubviewShowingCount should be inc'ed");
  onButtonCommandExpectedButtonID = panelViewButtonIDPanel;
//   EventUtils.synthesizeMouseAtCenter(panelViewButtonNodePanel, {});
  panelViewButtonNodePanel.click();
  await promisePageActionPanelHidden();
  Assert.equal(onActionCommandCallCount, 0,
               "onActionCommandCallCount should remain 0");
  Assert.equal(onButtonCommandCallCount, 1,
               "onButtonCommandCallCount should be inc'ed");

  // Click the action's urlbar button, which should open the activated-action
  // panel showing the subview, and click the subview's first button.
  onSubviewPlacedExpectedPanelViewID = panelViewIDUrlbar;
  onSubviewShowingExpectedPanelViewID = panelViewIDUrlbar;
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onSubviewPlacedCount, 2,
               "onSubviewPlacedCount should be inc'ed");
  Assert.equal(onSubviewShowingCount, 2,
               "onSubviewShowingCount should be inc'ed");
  let panelViewButtonNodeUrlbar =
    document.getElementById(panelViewButtonIDUrlbar);
  Assert.notEqual(panelViewButtonNodeUrlbar, null, "panelViewButtonNodeUrlbar");
  onButtonCommandExpectedButtonID = panelViewButtonIDUrlbar;
  EventUtils.synthesizeMouseAtCenter(panelViewButtonNodeUrlbar, {});
  await promisePanelHidden(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onButtonCommandCallCount, 2,
               "onButtonCommandCallCount should be inc'ed");

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
  let onIframeShownCount = 0;

  let panelButtonID = BrowserPageActions.panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions.urlbarButtonNodeIDForActionID(id);

  let action = PageActions.addAction(new PageActions.Action({
    iconURL: "chrome://browser/skin/mail.svg",
    id,
    shownInUrlbar: true,
    title: "Test iframe",
    wantsIframe: true,
    onCommand(event, buttonNode) {
      onCommandCallCount++;
    },
    onIframeShown(iframeNode, panelNode) {
      onIframeShownCount++;
      Assert.ok(iframeNode, "iframeNode should be non-null: " + iframeNode);
      Assert.equal(iframeNode.localName, "iframe", "iframe localName");
      Assert.ok(panelNode, "panelNode should be non-null: " + panelNode);
      Assert.equal(panelNode.id, BrowserPageActions._activatedActionPanelID,
                   "panelNode.id");
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
  }));

  Assert.equal(action.id, id, "id");
  Assert.equal(action.wantsIframe, true, "wantsIframe");

  Assert.equal(onPlacedInPanelCallCount, 1,
               "onPlacedInPanelCallCount should be inc'ed");
  Assert.equal(onPlacedInUrlbarCallCount, 1,
               "onPlacedInUrlbarCallCount should be inc'ed");
  Assert.equal(onIframeShownCount, 0,
               "onIframeShownCount should remain 0");
  Assert.equal(onCommandCallCount, 0,
               "onCommandCallCount should remain 0");

  // The action's panel button should have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");

  // The action's urlbar button should have been created.
  let urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.notEqual(urlbarButtonNode, null, "urlbarButtonNode");

  // The button should have been inserted before the bookmark star.
  Assert.notEqual(urlbarButtonNode.nextSibling, null, "Should be a next node");
  Assert.equal(
    urlbarButtonNode.nextSibling.id,
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK).urlbarIDOverride,
    "Next node should be the bookmark star"
  );

  // Open the panel, click the action's button.
  await promisePageActionPanelOpen();
  Assert.equal(onIframeShownCount, 0, "onIframeShownCount should remain 0");
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");
  Assert.equal(onIframeShownCount, 1, "onIframeShownCount should be inc'ed");

  // The activated-action panel should have opened, anchored to the action's
  // urlbar button.
  let aaPanel =
    document.getElementById(BrowserPageActions._activatedActionPanelID);
  Assert.notEqual(aaPanel, null, "activated-action panel");
  Assert.equal(aaPanel.anchorNode.id, urlbarButtonID, "aaPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelHidden(BrowserPageActions._activatedActionPanelID);

  // Click the action's urlbar button.
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");
  Assert.equal(onIframeShownCount, 2, "onIframeShownCount should be inc'ed");

  // The activated-action panel should have opened, again anchored to the
  // action's urlbar button.
  aaPanel = document.getElementById(BrowserPageActions._activatedActionPanelID);
  Assert.notEqual(aaPanel, null, "aaPanel");
  Assert.equal(aaPanel.anchorNode.id, urlbarButtonID, "aaPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelHidden(BrowserPageActions._activatedActionPanelID);

  // Hide the action's button in the urlbar.
  action.shownInUrlbar = false;
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  // Open the panel, click the action's button.
  await promisePageActionPanelOpen();
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePanelShown(BrowserPageActions._activatedActionPanelID);
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");
  Assert.equal(onIframeShownCount, 3, "onIframeShownCount should be inc'ed");

  // The activated-action panel should have opened, this time anchored to the
  // main page action button in the urlbar.
  aaPanel = document.getElementById(BrowserPageActions._activatedActionPanelID);
  Assert.notEqual(aaPanel, null, "aaPanel");
  Assert.equal(aaPanel.anchorNode.id, BrowserPageActions.mainButtonNode.id,
               "aaPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePanelHidden(BrowserPageActions._activatedActionPanelID);

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

  let initialActions = PageActions.actions;
  let initialBuiltInActions = PageActions.builtInActions;
  let initialNonBuiltInActions = PageActions.nonBuiltInActions;
  let initialBookmarkSeparatorIndex = PageActions.actions.findIndex(a => {
    return a.id == PageActions.ACTION_ID_BOOKMARK_SEPARATOR;
  });

  let action = PageActions.addAction(new PageActions.Action({
    id,
    title: "Test insertBeforeActionID",
    _insertBeforeActionID: PageActions.ACTION_ID_BOOKMARK_SEPARATOR,
  }));

  Assert.equal(action.id, id, "id");
  Assert.ok("__insertBeforeActionID" in action, "__insertBeforeActionID");
  Assert.equal(action.__insertBeforeActionID,
               PageActions.ACTION_ID_BOOKMARK_SEPARATOR,
               "action.__insertBeforeActionID");

  Assert.equal(PageActions.actions.length,
               initialActions.length + 1,
               "PageActions.actions.length should be updated");
  Assert.equal(PageActions.builtInActions.length,
               initialBuiltInActions.length + 1,
               "PageActions.builtInActions.length should be updated");
  Assert.equal(PageActions.nonBuiltInActions.length,
               initialNonBuiltInActions.length,
               "PageActions.nonBuiltInActions.length should be updated");

  let actionIndex = PageActions.actions.findIndex(a => a.id == id);
  Assert.equal(initialBookmarkSeparatorIndex, actionIndex,
               "initialBookmarkSeparatorIndex");
  let newBookmarkSeparatorIndex = PageActions.actions.findIndex(a => {
    return a.id == PageActions.ACTION_ID_BOOKMARK_SEPARATOR;
  });
  Assert.equal(newBookmarkSeparatorIndex, initialBookmarkSeparatorIndex + 1,
               "newBookmarkSeparatorIndex");

  // The action's panel button should have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");

  // The button's next sibling should be the bookmark separator.
  Assert.notEqual(panelButtonNode.nextSibling, null,
                  "panelButtonNode.nextSibling");
  Assert.equal(
    panelButtonNode.nextSibling.id,
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BOOKMARK_SEPARATOR
    ),
    "panelButtonNode.nextSibling.id"
  );

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


// Tests that the ordering of multiple non-built-in actions is alphabetical.
add_task(async function multipleNonBuiltInOrdering() {
  let idPrefix = "test-multipleNonBuiltInOrdering-";
  let titlePrefix = "Test multipleNonBuiltInOrdering ";

  let initialActions = PageActions.actions;
  let initialBuiltInActions = PageActions.builtInActions;
  let initialNonBuiltInActions = PageActions.nonBuiltInActions;

  // Create some actions in an out-of-order order.
  let actions = [2, 1, 4, 3].map(index => {
    return PageActions.addAction(new PageActions.Action({
      id: idPrefix + index,
      title: titlePrefix + index,
    }));
  });

  // + 1 for the separator between built-in and non-built-in actions.
  Assert.equal(PageActions.actions.length,
               initialActions.length + actions.length + 1,
               "PageActions.actions.length should be updated");

  Assert.equal(PageActions.builtInActions.length,
               initialBuiltInActions.length,
               "PageActions.builtInActions.length should be same");
  Assert.equal(PageActions.nonBuiltInActions.length,
               initialNonBuiltInActions.length + actions.length,
               "PageActions.nonBuiltInActions.length should be updated");

  // Look at the final actions.length actions in PageActions.actions, from first
  // to last.
  for (let i = 0; i < actions.length; i++) {
    let expectedIndex = i + 1;
    let actualAction = PageActions.nonBuiltInActions[i];
    Assert.equal(actualAction.id, idPrefix + expectedIndex,
                 "actualAction.id for index: " + i);
  }

  // Check the button nodes in the panel.
  let expectedIndex = 1;
  let buttonNode = document.getElementById(
    BrowserPageActions.panelButtonNodeIDForActionID(idPrefix + expectedIndex)
  );
  Assert.notEqual(buttonNode, null, "buttonNode");
  Assert.notEqual(buttonNode.previousSibling, null,
                  "buttonNode.previousSibling");
  Assert.equal(
    buttonNode.previousSibling.id,
    BrowserPageActions.panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    ),
    "buttonNode.previousSibling.id"
  );
  for (let i = 0; i < actions.length; i++) {
    Assert.notEqual(buttonNode, null, "buttonNode at index: " + i);
    Assert.equal(
      buttonNode.id,
      BrowserPageActions.panelButtonNodeIDForActionID(idPrefix + expectedIndex),
      "buttonNode.id at index: " + i
    );
    buttonNode = buttonNode.nextSibling;
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

  // Remove all actions.
  for (let action of initialActions) {
    action.remove();
  }

  // Check the actions.
  Assert.deepEqual(PageActions.actions.map(a => a.id), [],
                   "PageActions.actions should be empty");
  Assert.deepEqual(PageActions.builtInActions.map(a => a.id), [],
                   "PageActions.builtInActions should be empty");
  Assert.deepEqual(PageActions.nonBuiltInActions.map(a => a.id), [],
                   "PageActions.nonBuiltInActions should be empty");

  // Check the panel.
  Assert.equal(BrowserPageActions.mainViewBodyNode.childNodes.length, 0,
               "All nodes should be gone");

  // Add a non-built-in action.
  let action = PageActions.addAction(new PageActions.Action({
    id: "test-nonBuiltFirst",
    title: "Test nonBuiltFirst",
  }));

  // Check the actions.
  Assert.deepEqual(PageActions.actions.map(a => a.id), [action.id],
                   "Action should be in PageActions.actions");
  Assert.deepEqual(PageActions.builtInActions.map(a => a.id), [],
                   "PageActions.builtInActions should be empty");
  Assert.deepEqual(PageActions.nonBuiltInActions.map(a => a.id), [action.id],
                   "Action should be in PageActions.nonBuiltInActions");

  // Check the panel.
  Assert.deepEqual(
    Array.map(BrowserPageActions.mainViewBodyNode.childNodes, n => n.id),
    [BrowserPageActions.panelButtonNodeIDForActionID(action.id)],
    "Action should be in panel"
  );

  // Now add back all the actions.
  for (let a of initialActions) {
    PageActions.addAction(a);
  }

  // Check the actions.
  Assert.deepEqual(
    PageActions.actions.map(a => a.id),
    initialActions.map(a => a.id).concat(
      [PageActions.ACTION_ID_BUILT_IN_SEPARATOR],
      [action.id]
    ),
    "All actions should be in PageActions.actions"
  );
  Assert.deepEqual(
    PageActions.builtInActions.map(a => a.id),
    initialActions.map(a => a.id),
    "PageActions.builtInActions should be initial actions"
  );
  Assert.deepEqual(
    PageActions.nonBuiltInActions.map(a => a.id),
    [action.id],
    "PageActions.nonBuiltInActions should contain action"
  );

  // Check the panel.
  Assert.deepEqual(
    Array.map(BrowserPageActions.mainViewBodyNode.childNodes, n => n.id),
    initialActions.map(a => a.id).concat(
      [PageActions.ACTION_ID_BUILT_IN_SEPARATOR],
      [action.id]
    ).map(id => BrowserPageActions.panelButtonNodeIDForActionID(id)),
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
    PageActions.builtInActions.map(a => a.id),
    initialActions.map(a => a.id),
    "PageActions.builtInActions should be initial actions"
  );
  Assert.deepEqual(
    PageActions.nonBuiltInActions.map(a => a.id),
    [],
    "Action should no longer be in PageActions.nonBuiltInActions"
  );

  // Check the panel.
  Assert.deepEqual(
    Array.map(BrowserPageActions.mainViewBodyNode.childNodes, n => n.id),
    initialActions.map(a => BrowserPageActions.panelButtonNodeIDForActionID(a.id)),
    "Action should no longer be in panel"
  );
});


// Makes sure that urlbar nodes appear in the correct order in a new window.
add_task(async function urlbarOrderNewWindow() {
  // Make some new actions.
  let actions = [0, 1, 2].map(i => {
    return PageActions.addAction(new PageActions.Action({
      id: `test-urlbarOrderNewWindow-${i}`,
      title: `Test urlbarOrderNewWindow ${i}`,
      shownInUrlbar: true,
    }));
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
    PageActions.actionsInUrlbar.slice(
      PageActions.actionsInUrlbar.length - (actions.length + 1)
    ).map(a => a.id),
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
  for (let node = win.BrowserPageActions.mainButtonNode.nextSibling;
       node;
       node = node.nextSibling) {
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
  // Add the bookmark action first to make sure it ends up last after migration.
  // Also include a non-default action (copyURL) to make sure we're not
  // accidentally testing default behavior.
  let ids = [
    PageActions.ACTION_ID_BOOKMARK,
    "pocket",
    "copyURL",
  ];
  let persisted = ids.reduce((memo, id) => {
    memo.ids[id] = true;
    memo.idsInUrlbar.push(id);
    return memo;
  }, { ids: {}, idsInUrlbar: [] });

  Services.prefs.setStringPref(
    PageActions.PREF_PERSISTED_ACTIONS,
    JSON.stringify(persisted)
  );

  // Migrate.
  PageActions._loadPersistedActions();

  Assert.equal(PageActions._persistedActions.version, 1, "Correct version");

  // Need to set copyURL's _shownInUrlbar.  It won't be set since it's false by
  // default and we reached directly into persisted storage above.
  PageActions.actionForID("copyURL")._shownInUrlbar = true;

  // expected order
  let orderedIDs = [
    "pocket",
    "copyURL",
    PageActions.ACTION_ID_BOOKMARK,
  ];

  // Check the ordering.
  Assert.deepEqual(
    PageActions._persistedActions.idsInUrlbar,
    orderedIDs,
    "PageActions._persistedActions.idsInUrlbar has right order"
  );
  Assert.deepEqual(
    PageActions.actionsInUrlbar.map(a => a.id),
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
  for (let node = win.BrowserPageActions.mainButtonNode.nextSibling;
       node;
       node = node.nextSibling) {
    actualUrlbarNodeIDs.push(node.id);
  }

  // Now check that they're in the right order.
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    orderedIDs.map(id => win.BrowserPageActions.urlbarButtonNodeIDForActionID(id)),
    "Expected actions in new window's urlbar"
  );

  // Done, clean up.
  await BrowserTestUtils.closeWindow(win);
  Services.prefs.clearUserPref(PageActions.PREF_PERSISTED_ACTIONS);
  PageActions.actionForID("copyURL").shownInUrlbar = false;
});


// Opens a new browser window and makes sure per-window state works right.
add_task(async function perWindowState() {
  // Add a test action.
  let title = "Test perWindowState";
  let action = PageActions.addAction(new PageActions.Action({
    iconURL: "chrome://browser/skin/mail.svg",
    id: "test-perWindowState",
    shownInUrlbar: true,
    title,
  }));

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
  Assert.equal(action.getTitle(), newGlobalTitle,
               "Title: global");
  Assert.equal(action.getTitle(window), newGlobalTitle,
               "Title: old window");
  Assert.equal(action.getTitle(newWindow), newGlobalTitle,
               "Title: new window");

  // The action's panel button nodes should be updated in both windows.
  let panelButtonID =
    BrowserPageActions.panelButtonNodeIDForActionID(action.id);
  for (let win of [window, newWindow]) {
    let panelButtonNode = win.document.getElementById(panelButtonID);
    Assert.equal(panelButtonNode.getAttribute("label"), newGlobalTitle,
                 "Panel button label should be global title");
  }

  // Set a new title in the new window.
  let newPerWinTitle = title + " new title in new window";
  action.setTitle(newPerWinTitle, newWindow);
  Assert.equal(action.getTitle(), newGlobalTitle,
               "Title: global should remain same");
  Assert.equal(action.getTitle(window), newGlobalTitle,
               "Title: old window should remain same");
  Assert.equal(action.getTitle(newWindow), newPerWinTitle,
               "Title: new window should be new");

  // The action's panel button node should be updated in the new window but the
  // same in the old window.
  let panelButtonNode1 = document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode1.getAttribute("label"), newGlobalTitle,
               "Panel button label in old window");
  let panelButtonNode2 = newWindow.document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode2.getAttribute("label"), newPerWinTitle,
               "Panel button label in new window");

  // Done, clean up.
  await BrowserTestUtils.closeWindow(newWindow);
  action.remove();
});


// Adds an action, changes its placement in the urlbar to something non-default,
// removes the action, and then adds it back.  Since the action was removed and
// re-added without restarting the app (or more accurately without calling
// PageActions._purgeUnregisteredPersistedActions), the action should remain in
// persisted state and retain its last placement in the urlbar.
add_task(async function removeRetainState() {
  // Get the list of actions initially in the urlbar.
  let initialActionsInUrlbar = PageActions.actionsInUrlbar;
  Assert.ok(initialActionsInUrlbar.length > 0,
            "This test expects there to be at least one action in the urlbar initially (like the bookmark star)");

  // Add a test action.
  let id = "test-removeRetainState";
  let testAction = PageActions.addAction(new PageActions.Action({
    id,
    title: "Test removeRetainState",
  }));

  // Show its button in the urlbar.
  testAction.shownInUrlbar = true;

  // "Move" the test action to the front of the urlbar by toggling shownInUrlbar
  // for all the other actions in the urlbar.
  for (let action of initialActionsInUrlbar) {
    action.shownInUrlbar = false;
    action.shownInUrlbar = true;
  }

  // Check the actions in PageActions.actionsInUrlbar.
  Assert.deepEqual(
    PageActions.actionsInUrlbar.map(a => a.id),
    [testAction].concat(initialActionsInUrlbar).map(a => a.id),
    "PageActions.actionsInUrlbar should be in expected order: testAction followed by all initial actions"
  );

  // Check the nodes in the urlbar.
  let actualUrlbarNodeIDs = [];
  for (let node = BrowserPageActions.mainButtonNode.nextSibling;
       node;
       node = node.nextSibling) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    [testAction].concat(initialActionsInUrlbar).map(a => BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)),
    "urlbar nodes should be in expected order: testAction followed by all initial actions"
  );

  // Remove the test action.
  testAction.remove();

  // Check the actions in PageActions.actionsInUrlbar.
  Assert.deepEqual(
    PageActions.actionsInUrlbar.map(a => a.id),
    initialActionsInUrlbar.map(a => a.id),
    "PageActions.actionsInUrlbar should be in expected order after removing test action: all initial actions"
  );

  // Check the nodes in the urlbar.
  actualUrlbarNodeIDs = [];
  for (let node = BrowserPageActions.mainButtonNode.nextSibling;
       node;
       node = node.nextSibling) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    initialActionsInUrlbar.map(a => BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)),
    "urlbar nodes should be in expected order after removing test action: all initial actions"
  );

  // Add the test action again.
  testAction = PageActions.addAction(new PageActions.Action({
    id,
    title: "Test removeRetainState",
  }));

  // Show its button in the urlbar again.
  testAction.shownInUrlbar = true;

  // Check the actions in PageActions.actionsInUrlbar.
  Assert.deepEqual(
    PageActions.actionsInUrlbar.map(a => a.id),
    [testAction].concat(initialActionsInUrlbar).map(a => a.id),
    "PageActions.actionsInUrlbar should be in expected order after re-adding test action: testAction followed by all initial actions"
  );

  // Check the nodes in the urlbar.
  actualUrlbarNodeIDs = [];
  for (let node = BrowserPageActions.mainButtonNode.nextSibling;
       node;
       node = node.nextSibling) {
    actualUrlbarNodeIDs.push(node.id);
  }
  Assert.deepEqual(
    actualUrlbarNodeIDs,
    [testAction].concat(initialActionsInUrlbar).map(a => BrowserPageActions.urlbarButtonNodeIDForActionID(a.id)),
    "urlbar nodes should be in expected order after re-adding test action: testAction followed by all initial actions"
  );

  // Done, clean up.
  testAction.remove();
});


function promisePageActionPanelOpen() {
  let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils);
  return BrowserTestUtils.waitForCondition(() => {
    // Wait for the main page action button to become visible.  It's hidden for
    // some URIs, so depending on when this is called, it may not yet be quite
    // visible.  It's up to the caller to make sure it will be visible.
    info("Waiting for main page action button to have non-0 size");
    let bounds = dwu.getBoundsWithoutFlushing(BrowserPageActions.mainButtonNode);
    return bounds.width > 0 && bounds.height > 0;
  }).then(() => {
    // Wait for the panel to become open, by clicking the button if necessary.
    info("Waiting for main page action panel to be open");
    if (BrowserPageActions.panelNode.state == "open") {
      return Promise.resolve();
    }
    let shownPromise = promisePageActionPanelShown();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    return shownPromise;
  }).then(() => {
    // Wait for items in the panel to become visible.
    return promisePageActionViewChildrenVisible(BrowserPageActions.mainViewNode);
  });
}

function promisePageActionPanelShown() {
  return promisePanelShown(BrowserPageActions.panelNode);
}

function promisePageActionPanelHidden() {
  return promisePanelHidden(BrowserPageActions.panelNode);
}

function promisePanelShown(panelIDOrNode) {
  return promisePanelEvent(panelIDOrNode, "popupshown");
}

function promisePanelHidden(panelIDOrNode) {
  return promisePanelEvent(panelIDOrNode, "popuphidden");
}

function promisePanelEvent(panelIDOrNode, eventType) {
  return new Promise(resolve => {
    let panel = typeof(panelIDOrNode) != "string" ? panelIDOrNode :
                document.getElementById(panelIDOrNode);
    if (!panel ||
        (eventType == "popupshown" && panel.state == "open") ||
        (eventType == "popuphidden" && panel.state == "closed")) {
      executeSoon(resolve);
      return;
    }
    panel.addEventListener(eventType, () => {
      executeSoon(resolve);
    }, { once: true });
  });
}

function promisePageActionViewShown() {
  info("promisePageActionViewShown waiting for ViewShown");
  return BrowserTestUtils.waitForEvent(BrowserPageActions.panelNode, "ViewShown").then(async event => {
    let panelViewNode = event.originalTarget;
    await promisePageActionViewChildrenVisible(panelViewNode);
    return panelViewNode;
  });
}

function promisePageActionViewChildrenVisible(panelViewNode) {
  info("promisePageActionViewChildrenVisible waiting for a child node to be visible");
  let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils);
  return BrowserTestUtils.waitForCondition(() => {
    let bodyNode = panelViewNode.firstChild;
    for (let childNode of bodyNode.childNodes) {
      let bounds = dwu.getBoundsWithoutFlushing(childNode);
      if (bounds.width > 0 && bounds.height > 0) {
        return true;
      }
    }
    return false;
  });
}
