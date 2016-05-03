/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals AddonsPanel, WorkersPanel */

"use strict";

const { createFactory, createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const PanelMenu = createFactory(require("./panel-menu"));

loader.lazyGetter(this, "AddonsPanel",
  () => createFactory(require("./addons/panel")));
loader.lazyGetter(this, "WorkersPanel",
  () => createFactory(require("./workers/panel")));

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const panels = [{
  id: "addons",
  panelId: "addons-panel",
  name: Strings.GetStringFromName("addons"),
  icon: "chrome://devtools/skin/images/debugging-addons.svg",
  component: AddonsPanel
}, {
  id: "workers",
  panelId: "workers-panel",
  name: Strings.GetStringFromName("workers"),
  icon: "chrome://devtools/skin/images/debugging-workers.svg",
  component: WorkersPanel
}];

const defaultPanelId = "addons";

module.exports = createClass({
  displayName: "AboutDebuggingApp",

  getInitialState() {
    return {
      selectedPanelId: defaultPanelId
    };
  },

  componentDidMount() {
    window.addEventListener("hashchange", this.onHashChange);
    this.onHashChange();
    this.props.telemetry.toolOpened("aboutdebugging");
  },

  componentWillUnmount() {
    window.removeEventListener("hashchange", this.onHashChange);
    this.props.telemetry.toolClosed("aboutdebugging");
    this.props.telemetry.destroy();
  },

  onHashChange() {
    let hash = window.location.hash;
    // Default to defaultTabId if no hash is provided.
    let panelId = hash ? hash.substr(1) : defaultPanelId;

    let isValid = panels.some(p => p.id == panelId);
    if (isValid) {
      this.setState({ selectedPanelId: panelId });
    } else {
      // If the current hash matches no valid category, navigate to the default
      // panel.
      this.selectPanel(defaultPanelId);
    }
  },

  selectPanel(panelId) {
    window.location.hash = "#" + panelId;
  },

  render() {
    let { client } = this.props;
    let { selectedPanelId } = this.state;
    let selectPanel = this.selectPanel;

    let selectedPanel = panels.find(p => p.id == selectedPanelId);

    return dom.div({ className: "app" },
      PanelMenu({ panels, selectedPanelId, selectPanel }),
      dom.div({ className: "main-content" },
        selectedPanel.component({ client, id: selectedPanel.panelId })
      )
    );
  }
});
