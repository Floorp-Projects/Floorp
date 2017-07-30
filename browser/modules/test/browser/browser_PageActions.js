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
    await BrowserTestUtils.removeTab(tab);
  });
});


// Tests a simple non-built-in action without an iframe or subview.  Also
// thoroughly checks most of the action's properties, methods, and DOM nodes, so
// it's not necessary to do that in general in other test tasks.
add_task(async function simple() {
  let iconURL = "chrome://browser/skin/email-link.svg";
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

  let panelButtonID = BrowserPageActions._panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions._urlbarButtonNodeIDForActionID(id);

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

  Assert.equal(action.iconURL, iconURL, "iconURL");
  Assert.equal(action.id, id, "id");
  Assert.deepEqual(action.nodeAttributes, nodeAttributes, "nodeAttributes");
  Assert.equal(action.shownInUrlbar, false, "shownInUrlbar");
  Assert.equal(action.subview, null, "subview");
  Assert.equal(action.title, title, "title");
  Assert.equal(action.tooltip, tooltip, "tooltip");
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

  // The action's panel button should have been created.
  let panelButtonNode = document.getElementById(panelButtonID);
  Assert.notEqual(panelButtonNode, null, "panelButtonNode");
  Assert.equal(panelButtonNode.getAttribute("label"), action.title, "label");
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
    BrowserPageActions._panelButtonNodeIDForActionID(
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
  onCommandExpectedButtonID = urlbarButtonID;
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  Assert.equal(onCommandCallCount, 2, "onCommandCallCount should be inc'ed");

  // Set a new title.
  let newTitle = title + " new title";
  action.title = newTitle;
  Assert.equal(action.title, newTitle, "New title");
  Assert.equal(panelButtonNode.getAttribute("label"), action.title, "New label");

  // Remove the action.
  action.remove();
  panelButtonNode = document.getElementById(panelButtonID);
  Assert.equal(panelButtonNode, null, "panelButtonNode");
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  Assert.deepEqual(PageActions.actions, initialActions,
                   "Actions should go back to initial");
  Assert.equal(PageActions.actionForID(action.id), null,
               "actionForID should be null");

  // The separator between the built-in actions and non-built-in actions should
  // be gone now, too.
  let separatorNode = document.getElementById(
    BrowserPageActions._panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    )
  );
  Assert.equal(separatorNode, null, "No separator");
  Assert.ok(!BrowserPageActions.mainViewBodyNode
            .lastChild.localName.includes("separator"),
            "Last child should not be separator");
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

  let panelButtonID = BrowserPageActions._panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions._urlbarButtonNodeIDForActionID(id);

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
    iconURL: "chrome://browser/skin/email-link.svg",
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

  // Click the action's urlbar button, which should open the temp panel showing
  // the subview, and click the subview's first button.
  onSubviewPlacedExpectedPanelViewID = panelViewIDUrlbar;
  onSubviewShowingExpectedPanelViewID = panelViewIDUrlbar;
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelShown(BrowserPageActions._tempPanelID);
  Assert.equal(onSubviewPlacedCount, 2,
               "onSubviewPlacedCount should be inc'ed");
  Assert.equal(onSubviewShowingCount, 2,
               "onSubviewShowingCount should be inc'ed");
  let panelViewButtonNodeUrlbar =
    document.getElementById(panelViewButtonIDUrlbar);
  Assert.notEqual(panelViewButtonNodeUrlbar, null, "panelViewButtonNodeUrlbar");
  onButtonCommandExpectedButtonID = panelViewButtonIDUrlbar;
  EventUtils.synthesizeMouseAtCenter(panelViewButtonNodeUrlbar, {});
  await promisePanelHidden(BrowserPageActions._tempPanelID);
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

  let panelButtonID = BrowserPageActions._panelButtonNodeIDForActionID(id);
  let urlbarButtonID = BrowserPageActions._urlbarButtonNodeIDForActionID(id);

  let action = PageActions.addAction(new PageActions.Action({
    iconURL: "chrome://browser/skin/email-link.svg",
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
      Assert.equal(panelNode.id, BrowserPageActions._tempPanelID,
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

  // Open the panel, click the action's button.
  await promisePageActionPanelOpen();
  Assert.equal(onIframeShownCount, 0, "onIframeShownCount should remain 0");
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePanelShown(BrowserPageActions._tempPanelID);
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");
  Assert.equal(onIframeShownCount, 1, "onIframeShownCount should be inc'ed");

  // The temp panel should have opened, anchored to the action's urlbar button.
  let tempPanel = document.getElementById(BrowserPageActions._tempPanelID);
  Assert.notEqual(tempPanel, null, "tempPanel");
  Assert.equal(tempPanel.anchorNode.id, urlbarButtonID,
               "tempPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelHidden(BrowserPageActions._tempPanelID);

  // Click the action's urlbar button.
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelShown(BrowserPageActions._tempPanelID);
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");
  Assert.equal(onIframeShownCount, 2, "onIframeShownCount should be inc'ed");

  // The temp panel should have opened, again anchored to the action's urlbar
  // button.
  tempPanel = document.getElementById(BrowserPageActions._tempPanelID);
  Assert.notEqual(tempPanel, null, "tempPanel");
  Assert.equal(tempPanel.anchorNode.id, urlbarButtonID,
               "tempPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(urlbarButtonNode, {});
  await promisePanelHidden(BrowserPageActions._tempPanelID);

  // Hide the action's button in the urlbar.
  action.shownInUrlbar = false;
  urlbarButtonNode = document.getElementById(urlbarButtonID);
  Assert.equal(urlbarButtonNode, null, "urlbarButtonNode");

  // Open the panel, click the action's button.
  await promisePageActionPanelOpen();
  EventUtils.synthesizeMouseAtCenter(panelButtonNode, {});
  await promisePanelShown(BrowserPageActions._tempPanelID);
  Assert.equal(onCommandCallCount, 0, "onCommandCallCount should remain 0");
  Assert.equal(onIframeShownCount, 3, "onIframeShownCount should be inc'ed");

  // The temp panel should have opened, this time anchored to the main page
  // action button in the urlbar.
  tempPanel = document.getElementById(BrowserPageActions._tempPanelID);
  Assert.notEqual(tempPanel, null, "tempPanel");
  Assert.equal(tempPanel.anchorNode.id, BrowserPageActions.mainButtonNode.id,
               "tempPanel.anchorNode.id");
  EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
  await promisePanelHidden(BrowserPageActions._tempPanelID);

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
  let panelButtonID = BrowserPageActions._panelButtonNodeIDForActionID(id);

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
    BrowserPageActions._panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BOOKMARK_SEPARATOR
    ),
    "panelButtonNode.nextSibling.id"
  );

  // The separator between the built-in and non-built-in actions should not have
  // been created.
  Assert.equal(
    document.getElementById(
      BrowserPageActions._panelButtonNodeIDForActionID(
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
    BrowserPageActions._panelButtonNodeIDForActionID(idPrefix + expectedIndex)
  );
  Assert.notEqual(buttonNode, null, "buttonNode");
  Assert.notEqual(buttonNode.previousSibling, null,
                  "buttonNode.previousSibling");
  Assert.equal(
    buttonNode.previousSibling.id,
    BrowserPageActions._panelButtonNodeIDForActionID(
      PageActions.ACTION_ID_BUILT_IN_SEPARATOR
    ),
    "buttonNode.previousSibling.id"
  );
  for (let i = 0; i < actions.length; i++) {
    Assert.notEqual(buttonNode, null, "buttonNode at index: " + i);
    Assert.equal(
      buttonNode.id,
      BrowserPageActions._panelButtonNodeIDForActionID(idPrefix + expectedIndex),
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
      BrowserPageActions._panelButtonNodeIDForActionID(
        PageActions.ACTION_ID_BUILT_IN_SEPARATOR
      )
    ),
    null,
    "Separator should be gone"
  );
});


function promisePageActionPanelOpen() {
  let button = document.getElementById("pageActionButton");
  let shownPromise = promisePageActionPanelShown();
  EventUtils.synthesizeMouseAtCenter(button, {});
  return shownPromise;
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
        (eventType == "popupshowing" && panel.state == "open") ||
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
  return new Promise(resolve => {
    BrowserPageActions.panelNode.addEventListener("ViewShown", (event) => {
      let target = event.originalTarget;
      window.setTimeout(() => {
        resolve(target);
      }, 5000);
    }, { once: true });
  });
}
