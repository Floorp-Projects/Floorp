/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "data:text/html;charset=UTF-8," +
  "<h1>browser_inspector_addtabbar.js</h1>";

const CONTENT_TEXT = "Hello World!";

/**
 * Verify InspectorPanel.addSidebarTab() API that can be consumed
 * by DevTools extensions as well as DevTools code base.
 */
add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URI);

  const React = inspector.React;
  const { div } = React.DOM;

  info("Adding custom panel.");

  // Define custom side-panel.
  let tabPanel = React.createFactory(React.createClass({
    displayName: "myTabPanel",
    render: function () {
      return (
        div({className: "my-tab-panel"},
          CONTENT_TEXT
        )
      );
    }
  }));

  // Append custom panel (tab) into the Inspector panel and
  // make sure it's selected by default (the last arg = true).
  inspector.addSidebarTab("myPanel", "My Panel", tabPanel, true);
  is(inspector.sidebar.getCurrentTabID(), "myPanel",
     "My Panel is selected by default");

  // Define another custom side-panel.
  tabPanel = React.createFactory(React.createClass({
    displayName: "myTabPanel2",
    render: function () {
      return (
        div({className: "my-tab-panel2"},
          "Another Content"
        )
      );
    }
  }));

  // Append second panel, but don't select it by default.
  inspector.addSidebarTab("myPanel", "My Panel", tabPanel, false);
  is(inspector.sidebar.getCurrentTabID(), "myPanel",
     "My Panel is selected by default");

  // Check the the panel content is properly rendered.
  let tabPanelNode = inspector.panelDoc.querySelector(".my-tab-panel");
  is(tabPanelNode.textContent, CONTENT_TEXT,
    "Side panel content has been rendered.");
});
