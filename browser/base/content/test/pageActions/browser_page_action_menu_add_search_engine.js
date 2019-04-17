"use strict";

// Checks the panel button with a page that doesn't offer any engines.
add_task(async function none() {
  let url = "http://mochi.test:8888/";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();

    // The action should not be present.
    let actions = PageActions.actionsInPanel(window);
    Assert.ok(!actions.some(a => a.id == "addSearchEngine"),
              "Action should not be present in panel");
    let button =
      BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(!button, "Action button should not be in panel");
  });
});


// Checks the panel button with a page that offers one engine.
add_task(async function one() {
  let url = getRootDirectory(gTestPath) + "page_action_menu_add_search_engine_one.html";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // The action should be present.
    let actions = PageActions.actionsInPanel(window);
    let action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    let expectedTitle = "Add Search Engine";
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    let button =
      BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Button should be in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), false,
                 "Button should not expand into a subview");

    // Click the action's button.
    let enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_0");
    let hiddenPromise = promisePageActionPanelHidden();
    let feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await hiddenPromise;
    let engine = await enginePromise;
    let feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!");

    // Open the panel again.
    await promisePageActionPanelOpen();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();

    // The action should be gone.
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(!action, "Action should not be present in panel");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(!button, "Action button should not be in panel");

    // Remove the engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_0");
    await Services.search.removeEngine(engine);
    await enginePromise;

    // Open the panel again.
    await promisePageActionPanelOpen();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();

    // The action should be present again.
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Action button should be in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), false,
                 "Button should not expand into a subview");
  });
});


// Checks the panel button with a page that offers many engines.
add_task(async function many() {
  let url = getRootDirectory(gTestPath) + "page_action_menu_add_search_engine_many.html";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // The action should be present.
    let actions = PageActions.actionsInPanel(window);
    let action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    let expectedTitle = "Add Search Engine";
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    let button =
      BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Action button should be in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), true,
                 "Button should expand into a subview");

    // Click the action's button.  The subview should be shown.
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(button, {});
    let view = await viewPromise;
    let viewID =
       BrowserPageActions._panelViewNodeIDForActionID("addSearchEngine", false);
    Assert.equal(view.id, viewID, "View ID");
    let bodyID = viewID + "-body";
    let body = document.getElementById(bodyID);
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_0",
        "page_action_menu_add_search_engine_1",
        "page_action_menu_add_search_engine_2",
      ],
      "Subview children"
    );

    // Click the first engine to install it.
    let enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_0");
    let hiddenPromise = promisePageActionPanelHidden();
    let feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(body.children[0], {});
    await hiddenPromise;
    let engines = [];
    let engine = await enginePromise;
    engines.push(engine);
    let feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!", "Feedback text");

    // Open the panel and show the subview again.  The installed engine should
    // be gone.
    await promisePageActionPanelOpen();
    viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await viewPromise;
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_1",
        "page_action_menu_add_search_engine_2",
      ],
      "Subview children"
    );

    // Click the next engine to install it.
    enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_1");
    hiddenPromise = promisePageActionPanelHidden();
    feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(body.children[0], {});
    await hiddenPromise;
    engine = await enginePromise;
    engines.push(engine);
    feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!", "Feedback text");

    // Open the panel again.  This time the action button should show the one
    // remaining engine.
    await promisePageActionPanelOpen();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    expectedTitle = "Add Search Engine";
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Button should be present in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), false,
                 "Button should not expand into a subview");

    // Click the button.
    enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_2");
    hiddenPromise = promisePageActionPanelHidden();
    feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await hiddenPromise;
    engine = await enginePromise;
    engines.push(engine);
    feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!", "Feedback text");

    // All engines are installed at this point.  Open the panel and make sure
    // the action is gone.
    await promisePageActionPanelOpen();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(!action, "Action should be gone");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(!button, "Button should not be in panel");

    // Remove the first engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_0");
    await Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again.  The action should be present and showing the first
    // engine.
    await promisePageActionPanelOpen();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    expectedTitle = "Add Search Engine";
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Button should be present in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), false,
                 "Button should not expand into a subview");

    // Remove the second engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_1");
    await Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again and check the subview.  The subview should be
    // present now that there are two offered engines again.
    await promisePageActionPanelOpen();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    expectedTitle = "Add Search Engine";
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Button should be in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), true,
                 "Button should expand into a subview");
    viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await viewPromise;
    body = document.getElementById(bodyID);
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_0",
        "page_action_menu_add_search_engine_1",
      ],
      "Subview children"
    );
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();

    // Remove the third engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_2");
    await Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again and check the subview.
    await promisePageActionPanelOpen();
    viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await viewPromise;
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_0",
        "page_action_menu_add_search_engine_1",
        "page_action_menu_add_search_engine_2",
      ],
      "Subview children"
    );
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();
  });
});


// Checks the urlbar button with a page that offers one engine.
add_task(async function urlbarOne() {
  let url = getRootDirectory(gTestPath) + "page_action_menu_add_search_engine_one.html";
  await BrowserTestUtils.withNewTab(url, async () => {
    await promiseNodeVisible(BrowserPageActions.mainButtonNode);

    // Pin the action to the urlbar.
    let placedPromise = promisePlacedInUrlbar();
    PageActions.actionForID("addSearchEngine").pinnedToUrlbar = true;

    // It should be placed.
    let button = await placedPromise;
    let actions = PageActions.actionsInUrlbar(window);
    let action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in urlbar");
    Assert.ok(button, "Action button should be in urlbar");

    // Click the action's button.
    let enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_0");
    let feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(button, {});
    let engine = await enginePromise;
    let feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!");

    // The action should be gone.
    actions = PageActions.actionsInUrlbar(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(!action, "Action should not be present in urlbar");
    button = BrowserPageActions.urlbarButtonNodeForActionID("addSearchEngine");
    Assert.ok(!button, "Action button should not be in urlbar");

    // Remove the engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_0");
    placedPromise = promisePlacedInUrlbar();
    await Services.search.removeEngine(engine);
    await enginePromise;

    // The action should be present again.
    button = await placedPromise;
    actions = PageActions.actionsInUrlbar(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in urlbar");
    Assert.ok(button, "Action button should be in urlbar");

    // Clean up.
    PageActions.actionForID("addSearchEngine").pinnedToUrlbar = false;
    await BrowserTestUtils.waitForCondition(() => {
      return !BrowserPageActions.urlbarButtonNodeForActionID("addSearchEngine");
    });
  });
});


// Checks the urlbar button with a page that offers many engines.
add_task(async function urlbarMany() {
  let url = getRootDirectory(gTestPath) + "page_action_menu_add_search_engine_many.html";
  await BrowserTestUtils.withNewTab(url, async () => {
    await promiseNodeVisible(BrowserPageActions.mainButtonNode);

    // Pin the action to the urlbar.
    let placedPromise = promisePlacedInUrlbar();
    PageActions.actionForID("addSearchEngine").pinnedToUrlbar = true;

    // It should be placed.
    let button = await placedPromise;
    let actions = PageActions.actionsInUrlbar(window);
    let action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in urlbar");
    Assert.ok(button, "Action button should be in urlbar");

    // Click the action's button.  The activated-action panel should open, and
    // it should contain the addSearchEngine subview.
    EventUtils.synthesizeMouseAtCenter(button, {});
    let view = await waitForActivatedActionPanel();
    let viewID =
       BrowserPageActions._panelViewNodeIDForActionID("addSearchEngine", true);
    Assert.equal(view.id, viewID, "View ID");
    let body = view.firstElementChild;
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_0",
        "page_action_menu_add_search_engine_1",
        "page_action_menu_add_search_engine_2",
      ],
      "Subview children"
    );

    // Click the first engine to install it.
    let enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_0");
    let hiddenPromise =
      promisePanelHidden(BrowserPageActions.activatedActionPanelNode);
    let feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(body.children[0], {});
    await hiddenPromise;
    let engines = [];
    let engine = await enginePromise;
    engines.push(engine);
    let feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!", "Feedback text");

    // Open the panel again.  The installed engine should be gone.
    EventUtils.synthesizeMouseAtCenter(button, {});
    view = await waitForActivatedActionPanel();
    body = view.firstElementChild;
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_1",
        "page_action_menu_add_search_engine_2",
      ],
      "Subview children"
    );

    // Click the next engine to install it.
    enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_1");
    hiddenPromise =
      promisePanelHidden(BrowserPageActions.activatedActionPanelNode);
    feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(body.children[0], {});
    await hiddenPromise;
    engine = await enginePromise;
    engines.push(engine);
    feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!", "Feedback text");

    // Now there's only one engine left, so clicking the button should simply
    // install it instead of opening the activated-action panel.
    enginePromise =
      promiseEngine("engine-added", "page_action_menu_add_search_engine_2");
    feedbackPromise = promiseFeedbackPanelShownAndHidden();
    EventUtils.synthesizeMouseAtCenter(button, {});
    engine = await enginePromise;
    engines.push(engine);
    feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Search engine added!", "Feedback text");

    // All engines are installed at this point.  The action should be gone.
    actions = PageActions.actionsInUrlbar(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(!action, "Action should be gone");
    button = BrowserPageActions.urlbarButtonNodeForActionID("addSearchEngine");
    Assert.ok(!button, "Button should not be in urlbar");

    // Remove the first engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_0");
    placedPromise = promisePlacedInUrlbar();
    await Services.search.removeEngine(engines.shift());
    await enginePromise;

    // The action should be placed again.
    button = await placedPromise;
    actions = PageActions.actionsInUrlbar(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in urlbar");
    Assert.ok(button, "Button should be in urlbar");

    // Remove the second engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_1");
    await Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again and check the subview.  The subview should be
    // present now that there are two offered engines again.
    EventUtils.synthesizeMouseAtCenter(button, {});
    view = await waitForActivatedActionPanel();
    body = view.firstElementChild;
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_0",
        "page_action_menu_add_search_engine_1",
      ],
      "Subview children"
    );

    // Hide the panel.
    hiddenPromise =
      promisePanelHidden(BrowserPageActions.activatedActionPanelNode);
    EventUtils.synthesizeMouseAtCenter(button, {});
    await hiddenPromise;

    // Remove the third engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_2");
    await Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again and check the subview.
    EventUtils.synthesizeMouseAtCenter(button, {});
    view = await waitForActivatedActionPanel();
    body = view.firstElementChild;
    Assert.deepEqual(
      Array.from(body.children, n => n.label),
      [
        "page_action_menu_add_search_engine_0",
        "page_action_menu_add_search_engine_1",
        "page_action_menu_add_search_engine_2",
      ],
      "Subview children"
    );

    // Hide the panel.
    hiddenPromise =
      promisePanelHidden(BrowserPageActions.activatedActionPanelNode);
    EventUtils.synthesizeMouseAtCenter(button, {});
    await hiddenPromise;

    // Clean up.
    PageActions.actionForID("addSearchEngine").pinnedToUrlbar = false;
    await BrowserTestUtils.waitForCondition(() => {
      return !BrowserPageActions.urlbarButtonNodeForActionID("addSearchEngine");
    });
  });
});


function promiseEngine(expectedData, expectedEngineName) {
  info(`Waiting for engine ${expectedData}`);
  return TestUtils.topicObserved("browser-search-engine-modified", (engine, data) => {
    info(`Got engine ${engine.wrappedJSObject.name} ${data}`);
    return expectedData == data &&
           expectedEngineName == engine.wrappedJSObject.name;
  }).then(([engine, data]) => engine);
}

function promiseFeedbackPanelShownAndHidden() {
  info("Waiting for feedback panel popupshown");
  return BrowserTestUtils.waitForEvent(ConfirmationHint._panel, "popupshown").then(() => {
    info("Got feedback panel popupshown. Now waiting for popuphidden");
    return BrowserTestUtils.waitForEvent(ConfirmationHint._panel, "popuphidden")
          .then(() => ConfirmationHint._message.textContent);
  });
}

function promisePlacedInUrlbar() {
  let action = PageActions.actionForID("addSearchEngine");
  return new Promise(resolve => {
    let onPlaced = action._onPlacedInUrlbar;
    action._onPlacedInUrlbar = button => {
      action._onPlacedInUrlbar = onPlaced;
      if (action._onPlacedInUrlbar) {
        action._onPlacedInUrlbar(button);
      }
      promiseNodeVisible(button).then(() => resolve(button));
    };
  });
}
