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

    let appMenuShownPromise = promisePanelElementShown(window, appMenu);
    await showInfoPromise("addons", "title", "text");
    await appMenuShownPromise;
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
  }
);

add_UITour_task(
  async function test_show_pageActionPanel_and_showInfo_buttonOnAppMenu() {
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
    is(
      pageActionPanel.state,
      "closed",
      "Check state of the page action panel if it was opened explictly by api user."
    );
    is(
      getShowInfoTargetName(),
      "privateWindow",
      "Should show info tooltip on the privateWindow button on the app menu"
    );

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
