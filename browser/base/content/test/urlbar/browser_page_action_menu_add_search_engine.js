"use strict";

// Checks a page that doesn't offer any engines.
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


// Checks a page that offers one engine.
add_task(async function one() {
  let url = getRootDirectory(gTestPath) + "page_action_menu_add_search_engine_one.html";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // The action should be present.
    let actions = PageActions.actionsInPanel(window);
    let action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    let expectedTitle =
      "Add \u{201C}page_action_menu_add_search_engine_0\u{201D} to One-Click Search";
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
    let feedbackPromise = promiseFeedbackPanelHidden();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await hiddenPromise;
    let engine = await enginePromise;
    let feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Added to Search Dropdown");

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
    Services.search.removeEngine(engine);
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


// Checks a page that offers many engines.
add_task(async function many() {
  let url = getRootDirectory(gTestPath) + "page_action_menu_add_search_engine_many.html";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // The action should be present.
    let actions = PageActions.actionsInPanel(window);
    let action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    let expectedTitle = "Add One-Click Search Engine";
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
      Array.map(body.childNodes, n => n.label),
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
    let feedbackPromise = promiseFeedbackPanelHidden();
    EventUtils.synthesizeMouseAtCenter(body.childNodes[0], {});
    await hiddenPromise;
    let engines = [];
    let engine = await enginePromise;
    engines.push(engine);
    let feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Added to Search Dropdown", "Feedback text");

    // Open the panel and show the subview again.  The installed engine should
    // be gone.
    await promisePageActionPanelOpen();
    viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await viewPromise;
    Assert.deepEqual(
      Array.map(body.childNodes, n => n.label),
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
    feedbackPromise = promiseFeedbackPanelHidden();
    EventUtils.synthesizeMouseAtCenter(body.childNodes[0], {});
    await hiddenPromise;
    engine = await enginePromise;
    engines.push(engine);
    feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Added to Search Dropdown", "Feedback text");

    // Open the panel again.  This time the action button should show the one
    // remaining engine.
    await promisePageActionPanelOpen();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    expectedTitle =
      "Add \u{201C}page_action_menu_add_search_engine_2\u{201D} to One-Click Search";
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
    feedbackPromise = promiseFeedbackPanelHidden();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await hiddenPromise;
    engine = await enginePromise;
    engines.push(engine);
    feedbackText = await feedbackPromise;
    Assert.equal(feedbackText, "Added to Search Dropdown", "Feedback text");

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
    Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again.  The action should be present and showing the first
    // engine.
    await promisePageActionPanelOpen();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    await promisePageActionPanelHidden();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    expectedTitle =
      "Add \u{201C}page_action_menu_add_search_engine_0\u{201D} to One-Click Search";
    Assert.equal(action.getTitle(window), expectedTitle, "Action title");
    button = BrowserPageActions.panelButtonNodeForActionID("addSearchEngine");
    Assert.ok(button, "Button should be present in panel");
    Assert.equal(button.label, expectedTitle, "Button label");
    Assert.equal(button.classList.contains("subviewbutton-nav"), false,
                 "Button should not expand into a subview");

    // Remove the second engine.
    enginePromise =
      promiseEngine("engine-removed", "page_action_menu_add_search_engine_1");
    Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again and check the subview.  The subview should be
    // present now that there are two offerred engines again.
    await promisePageActionPanelOpen();
    actions = PageActions.actionsInPanel(window);
    action = actions.find(a => a.id == "addSearchEngine");
    Assert.ok(action, "Action should be present in panel");
    expectedTitle = "Add One-Click Search Engine";
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
      Array.map(body.childNodes, n => n.label),
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
    Services.search.removeEngine(engines.shift());
    await enginePromise;

    // Open the panel again and check the subview.
    await promisePageActionPanelOpen();
    viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(button, {});
    await viewPromise;
    Assert.deepEqual(
      Array.map(body.childNodes, n => n.label),
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


function promiseEngine(expectedData, expectedEngineName) {
  return TestUtils.topicObserved("browser-search-engine-modified", (engine, data) => {
    return expectedData == data &&
           expectedEngineName == engine.wrappedJSObject.name;
  }).then(([engine, data]) => engine);
}

function promiseFeedbackPanelHidden() {
  return new Promise(resolve => {
    BrowserPageActionFeedback.panelNode.addEventListener("popuphidden", event => {
      resolve(BrowserPageActionFeedback.feedbackLabel.textContent);
    }, {once: true});
  });
}
