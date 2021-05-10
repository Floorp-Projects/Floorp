"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_UITour_task(
  async function test_highligh_between_buttonOnAppMenu_and_buttonOnPageActionPanel() {
    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    let appMenu = window.PanelUI.panel;
    let pageActionPanel = BrowserPageActions.panelNode;

    // Test highlighting the addons button on the app menu
    let appMenuShownPromise = promisePanelElementShown(window, appMenu);
    let highlightVisiblePromise = elementVisiblePromise(
      highlight,
      "Should show highlight"
    );
    gContentAPI.showHighlight("addons");
    await appMenuShownPromise;
    await highlightVisiblePromise;
    is(
      appMenu.state,
      "open",
      "Should open the app menu to highlight the addons button"
    );
    is(pageActionPanel.state, "closed", "Shouldn't open the page action panel");
    is(
      getShowHighlightTargetName(),
      "addons",
      "Should highlight the addons button on the app menu"
    );

    // Test switching the highlight to the copyURL button on the page action panel
    let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);

    // This doesn't exist in Proton.
    if (!UITour.targets.has("pageAction-copyURL")) {
      return;
    }

    let pageActionPanelShownPromise = promisePanelElementShown(
      window,
      pageActionPanel
    );
    highlightVisiblePromise = elementVisiblePromise(
      highlight,
      "Should show highlight"
    );
    gContentAPI.showHighlight("pageAction-copyURL");
    await appMenuHiddenPromise;
    await pageActionPanelShownPromise;
    await highlightVisiblePromise;
    is(
      appMenu.state,
      "closed",
      "Should close the app menu after no more highlight for the addons button"
    );
    is(
      pageActionPanel.state,
      "open",
      "Should open the page action panel to highlight the copyURL button"
    );
    is(
      getShowHighlightTargetName(),
      "pageAction-copyURL",
      "Should highlight the copyURL button on the page action panel"
    );

    // Test hiding highlight
    let pageActionPanelHiddenPromise = promisePanelElementHidden(
      window,
      pageActionPanel
    );
    let highlightHiddenPromise = elementHiddenPromise(
      highlight,
      "Should hide highlight"
    );
    gContentAPI.hideHighlight();
    await pageActionPanelHiddenPromise;
    await highlightHiddenPromise;
    is(
      appMenu.state,
      "closed",
      "Shouldn't open the app menu after hiding highlight"
    );
    is(
      pageActionPanel.state,
      "closed",
      "Should close the page action panel after hiding highlight"
    );
  }
);

add_UITour_task(
  async function test_showInfo_between_buttonOnPageActionPanel_and_buttonOnAppMenu() {
    let tooltip = document.getElementById("UITourTooltip");
    is_element_hidden(tooltip, "Tooltip should initially be hidden");

    let appMenu = window.PanelUI.panel;
    let pageActionPanel = BrowserPageActions.panelNode;
    let tooltipVisiblePromise = elementVisiblePromise(
      tooltip,
      "Should show info tooltip"
    );

    // This doesn't exist in Proton.
    if (UITour.targets.has("pageAction-emailLink")) {
      // Test showing info tooltip on the emailLink button on the page action panel
      let pageActionPanelShownPromise = promisePanelElementShown(
        window,
        pageActionPanel
      );
      await showInfoPromise("pageAction-emailLink", "title", "text");
      await pageActionPanelShownPromise;
      await tooltipVisiblePromise;
      is(appMenu.state, "closed", "Shouldn't open the app menu");
      is(
        pageActionPanel.state,
        "open",
        "Should open the page action panel to show info on the copyURL button"
      );
      is(
        getShowInfoTargetName(),
        "pageAction-emailLink",
        "Should show info tooltip on the emailLink button on the page action panel"
      );

      // Test switching info tooltip to the addons button on the app menu
      let appMenuShownPromise = promisePanelElementShown(window, appMenu);
      let pageActionPanelHiddenPromise = promisePanelElementHidden(
        window,
        pageActionPanel
      );

      tooltipVisiblePromise = elementVisiblePromise(
        tooltip,
        "Should show info tooltip"
      );
      await showInfoPromise("addons", "title", "text");
      await appMenuShownPromise;
      await pageActionPanelHiddenPromise;
    } else {
      let appMenuShownPromise = promisePanelElementShown(window, appMenu);
      await showInfoPromise("addons", "title", "text");
      await appMenuShownPromise;
    }
    await tooltipVisiblePromise;
    is(
      appMenu.state,
      "open",
      "Should open the app menu to show info on the addons button"
    );
    is(
      pageActionPanel.state,
      "closed",
      "Should close the page action panel after no more show info for the copyURL button"
    );
    is(
      getShowInfoTargetName(),
      "addons",
      "Should show info tooltip on the addons button on the app menu"
    );

    // Test hiding info tooltip
    let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
    let tooltipHiddenPromise = elementHiddenPromise(
      tooltip,
      "Should hide info"
    );
    gContentAPI.hideInfo();
    await appMenuHiddenPromise;
    await tooltipHiddenPromise;
    is(appMenu.state, "closed", "Should close the app menu after hiding info");
    is(
      pageActionPanel.state,
      "closed",
      "Shouldn't open the page action panel after hiding info"
    );
  }
);

add_UITour_task(
  async function test_highlight_buttonOnPageActionPanel_and_showInfo_buttonOnAppMenu() {
    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");
    let tooltip = document.getElementById("UITourTooltip");
    is_element_hidden(tooltip, "Tooltip should initially be hidden");

    let appMenu = window.PanelUI.panel;
    let pageActionPanel = BrowserPageActions.panelNode;
    let pageActionPanelHiddenPromise = Promise.resolve();

    // This doesn't exist in Proton.
    if (UITour.targets.has("pageAction-sendToDevice")) {
      // Test highlighting the sendToDevice button on the page action panel
      let pageActionPanelShownPromise = promisePanelElementShown(
        window,
        pageActionPanel
      );
      let highlightVisiblePromise = elementVisiblePromise(
        highlight,
        "Should show highlight"
      );
      gContentAPI.showHighlight("pageAction-sendToDevice");
      await pageActionPanelShownPromise;
      await highlightVisiblePromise;
      is(appMenu.state, "closed", "Shouldn't open the app menu");
      is(
        pageActionPanel.state,
        "open",
        "Should open the page action panel to highlight the sendToDevice button"
      );
      is(
        getShowHighlightTargetName(),
        "pageAction-sendToDevice",
        "Should highlight the sendToDevice button on the page action panel"
      );
      pageActionPanelHiddenPromise = promisePanelElementHidden(
        window,
        pageActionPanel
      );
    }

    // Test showing info tooltip on the privateWindow button on the app menu
    let appMenuShownPromise = promisePanelElementShown(window, appMenu);
    let tooltipVisiblePromise = elementVisiblePromise(
      tooltip,
      "Should show info tooltip"
    );
    let highlightHiddenPromise = elementHiddenPromise(
      highlight,
      "Should hide highlight"
    );
    await showInfoPromise("privateWindow", "title", "text");
    await appMenuShownPromise;
    await tooltipVisiblePromise;
    await pageActionPanelHiddenPromise;
    await highlightHiddenPromise;
    is(
      appMenu.state,
      "open",
      "Should open the app menu to show info on the privateWindow button"
    );
    is(pageActionPanel.state, "closed", "Should close the page action panel");
    is(
      getShowInfoTargetName(),
      "privateWindow",
      "Should show info tooltip on the privateWindow button on the app menu"
    );

    // Test hiding info tooltip
    let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
    let tooltipHiddenPromise = elementHiddenPromise(
      tooltip,
      "Should hide info"
    );
    gContentAPI.hideInfo();
    await appMenuHiddenPromise;
    await tooltipHiddenPromise;
    is(
      appMenu.state,
      "closed",
      "Should close the app menu after hiding info tooltip"
    );
  }
);

add_UITour_task(
  async function test_showInfo_buttonOnAppMenu_and_highlight_buttonOnPageActionPanel() {
    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");
    let tooltip = document.getElementById("UITourTooltip");
    is_element_hidden(tooltip, "Tooltip should initially be hidden");

    let appMenu = window.PanelUI.panel;
    let pageActionPanel = BrowserPageActions.panelNode;

    // Test showing info tooltip on the privateWindow button on the app menu
    let appMenuShownPromise = promisePanelElementShown(window, appMenu);
    let tooltipVisiblePromise = elementVisiblePromise(
      tooltip,
      "Should show info tooltip"
    );
    await showInfoPromise("privateWindow", "title", "text");
    await appMenuShownPromise;
    await tooltipVisiblePromise;
    is(
      appMenu.state,
      "open",
      "Should open the app menu to show info on the privateWindow button"
    );
    is(pageActionPanel.state, "closed", "Shouldn't open the page action panel");
    is(
      getShowInfoTargetName(),
      "privateWindow",
      "Should show info tooltip on the privateWindow button on the app menu"
    );

    // This doesn't exist in Proton.
    if (!UITour.targets.has("pageAction-sendToDevice")) {
      return;
    }

    // Test highlighting the sendToDevice button on the page action panel
    let pageActionPanelShownPromise = promisePanelElementShown(
      window,
      pageActionPanel
    );
    let highlightVisiblePromise = elementVisiblePromise(
      highlight,
      "Should show highlight"
    );
    let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
    let tooltipHiddenPromise = elementHiddenPromise(
      tooltip,
      "Should hide info"
    );
    gContentAPI.showHighlight("pageAction-sendToDevice");
    await pageActionPanelShownPromise;
    await highlightVisiblePromise;
    await appMenuHiddenPromise;
    await tooltipHiddenPromise;
    is(appMenu.state, "closed", "Should close the app menu");
    is(
      pageActionPanel.state,
      "open",
      "Should open the page action panel to highlight the sendToDevice button"
    );
    is(
      getShowHighlightTargetName(),
      "pageAction-sendToDevice",
      "Should highlight the sendToDevice button on the page action panel"
    );

    // Test hiding highlight
    let pageActionPanelHiddenPromise = promisePanelElementHidden(
      window,
      pageActionPanel
    );
    let highlightHiddenPromise = elementHiddenPromise(
      highlight,
      "Should hide highlight"
    );
    gContentAPI.hideHighlight();
    await pageActionPanelHiddenPromise;
    await highlightHiddenPromise;
    is(
      pageActionPanel.state,
      "closed",
      "Should close the page action panel after hiding highlight"
    );
  }
);

add_UITour_task(
  async function test_show_appMenu_and_highligh_buttonOnPageActionPanel() {
    // This doesn't exist in Proton.
    if (!UITour.targets.has("pageAction-sendToDevice")) {
      return;
    }

    let highlight = document.getElementById("UITourHighlight");
    is_element_hidden(highlight, "Highlight should initially be hidden");

    let appMenu = window.PanelUI.panel;
    let pageActionPanel = BrowserPageActions.panelNode;

    // Test explicity asking for opening the app menu
    let appMenuShownPromise = promisePanelElementShown(window, appMenu);
    gContentAPI.showMenu("appMenu");
    await appMenuShownPromise;
    is(appMenu.state, "open", "Should open the app menu");
    is(pageActionPanel.state, "closed", "Shouldn't open the page action panel");

    // Test highlighting the sendToDevice button on the page action panel
    let pageActionPanelShownPromise = promisePanelElementShown(
      window,
      pageActionPanel
    );
    let highlightVisiblePromise = elementVisiblePromise(
      highlight,
      "Should show highlight"
    );
    gContentAPI.showHighlight("pageAction-sendToDevice");
    await pageActionPanelShownPromise;
    await highlightVisiblePromise;
    is(
      appMenu.state,
      "open",
      "Shouldn't close the app menu because it is opened explictly by api user."
    );
    is(
      pageActionPanel.state,
      "open",
      "Should open the page action panel to highlight the sendToDevice button"
    );
    is(
      getShowHighlightTargetName(),
      "pageAction-sendToDevice",
      "Should highlight the sendToDevice button on the page action panel"
    );

    // Test hiding the app menu wouldn't affect the highlight on the page action panel
    let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
    gContentAPI.hideMenu("appMenu");
    await appMenuHiddenPromise;
    is_element_visible(highlight, "Highlight should still be visible");
    is(appMenu.state, "closed", "Should close the app menu");
    is(pageActionPanel.state, "open", "Shouldn't close the page action panel");
    is(
      getShowHighlightTargetName(),
      "pageAction-sendToDevice",
      "Should still highlight the sendToDevice button on the page action panel"
    );

    // Test hiding highlight
    let pageActionPanelHiddenPromise = promisePanelElementHidden(
      window,
      pageActionPanel
    );
    let highlightHiddenPromise = elementHiddenPromise(
      highlight,
      "Should hide highlight"
    );
    gContentAPI.hideHighlight();
    await pageActionPanelHiddenPromise;
    await highlightHiddenPromise;
    is(appMenu.state, "closed", "Shouldn't open the app menu");
    is(
      pageActionPanel.state,
      "closed",
      "Should close the page action panel after hiding highlight"
    );
  }
);

add_UITour_task(
  async function test_show_pageActionPanel_and_showInfo_buttonOnAppMenu() {
    let tooltip = document.getElementById("UITourTooltip");
    is_element_hidden(tooltip, "Tooltip should initially be hidden");

    let appMenu = window.PanelUI.panel;
    let pageActionPanel = BrowserPageActions.panelNode;

    if (!gProton) {
      // Test explicity asking for opening the page action panel
      let pageActionPanelShownPromise = promisePanelElementShown(
        window,
        pageActionPanel
      );
      gContentAPI.showMenu("pageActionPanel");
      await pageActionPanelShownPromise;
      is(appMenu.state, "closed", "Shouldn't open the app menu");
      is(pageActionPanel.state, "open", "Should open the page action panel");
    }

    // Test showing info tooltip on the privateWindow button on the app menu
    let appMenuShownPromise = promisePanelElementShown(window, appMenu);
    let tooltipVisiblePromise = elementVisiblePromise(
      tooltip,
      "Should show info tooltip"
    );
    await showInfoPromise("privateWindow", "title", "text");
    await appMenuShownPromise;
    await tooltipVisiblePromise;
    is(
      appMenu.state,
      "open",
      "Should open the app menu to show info on the privateWindow button"
    );
    is(
      pageActionPanel.state,
      gProton ? "closed" : "open",
      "Check state of the page action panel if it was opened explictly by api user."
    );
    is(
      getShowInfoTargetName(),
      "privateWindow",
      "Should show info tooltip on the privateWindow button on the app menu"
    );

    if (!gProton) {
      // Test hiding the page action panel wouldn't affect the info tooltip on the app menu
      let pageActionPanelHiddenPromise = promisePanelElementHidden(
        window,
        pageActionPanel
      );
      gContentAPI.hideMenu("pageActionPanel");
      await pageActionPanelHiddenPromise;
    }
    is_element_visible(tooltip, "Tooltip should still be visible");
    is(appMenu.state, "open", "Shouldn't close the app menu");
    is(
      pageActionPanel.state,
      "closed",
      "Should close the page action panel after hideMenu"
    );
    is(
      getShowInfoTargetName(),
      "privateWindow",
      "Should still show info tooltip on the privateWindow button on the app menu"
    );

    // Test hiding info tooltip
    let appMenuHiddenPromise = promisePanelElementHidden(window, appMenu);
    let tooltipHiddenPromise = elementHiddenPromise(
      tooltip,
      "Should hide info"
    );
    gContentAPI.hideInfo();
    await appMenuHiddenPromise;
    await tooltipHiddenPromise;
    is(appMenu.state, "closed", "Should close the app menu after hideInfo");
    is(pageActionPanel.state, "closed", "Shouldn't open the page action panel");
  }
);
