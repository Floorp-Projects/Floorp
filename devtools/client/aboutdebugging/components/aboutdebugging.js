/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createFactory, createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const PanelMenu = createFactory(require("./panel-menu"));

loader.lazyGetter(this, "AddonsPanel",
  () => createFactory(require("./addons/panel")));
loader.lazyGetter(this, "TabsPanel",
  () => createFactory(require("./tabs/panel")));
loader.lazyGetter(this, "WorkersPanel",
  () => createFactory(require("./workers/panel")));

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const panels = [{
  id: "addons",
  name: Strings.GetStringFromName("addons"),
  icon: "chrome://devtools/skin/images/debugging-addons.svg",
  component: AddonsPanel
}, {
  id: "tabs",
  name: Strings.GetStringFromName("tabs"),
  icon: "chrome://devtools/skin/images/debugging-tabs.svg",
  component: TabsPanel
}, {
  id: "workers",
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
    this.setState({
      selectedPanelId: window.location.hash.substr(1) || defaultPanelId
    });
  },

  selectPanel(panelId) {
    window.location.hash = "#" + panelId;
  },

  render() {
    let { client } = this.props;
    let { selectedPanelId } = this.state;
    let selectPanel = this.selectPanel;
    let selectedPanel = panels.find(p => p.id == selectedPanelId);
    let panel;

    if (selectedPanel) {
      panel = selectedPanel.component({ client, id: selectedPanel.id });
    } else {
      panel = (
        dom.div({ className: "error-page" },
          dom.h1({ className: "header-name" },
            Strings.GetStringFromName("pageNotFound")
          ),
          dom.h4({ className: "error-page-details" },
            Strings.formatStringFromName("doesNotExist", [selectedPanelId], 1))
        )
      );
    }

    return dom.div({ className: "app" },
      PanelMenu({ panels, selectedPanelId, selectPanel }),
      dom.div({ className: "main-content" }, panel)
    );
  }
});
