/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const Services = require("Services");

const React = require("devtools/client/shared/vendor/react");
const { TabMenu } = require("./tab-menu");

loader.lazyRequireGetter(this, "AddonsTab", "./components/addons-tab", true);
loader.lazyRequireGetter(this, "WorkersTab", "./components/workers-tab", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const tabs = [
  { id: "addons", name: Strings.GetStringFromName("addons"),
    icon: "chrome://devtools/skin/images/debugging-addons.svg",
    component: AddonsTab },
  { id: "workers", name: Strings.GetStringFromName("workers"),
    icon: "chrome://devtools/skin/images/debugging-workers.svg",
    component: WorkersTab },
];
const defaultTabId = "addons";

exports.AboutDebuggingApp = React.createClass({
  displayName: "AboutDebuggingApp",

  getInitialState() {
    return {
      selectedTabId: defaultTabId
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

  render() {
    let { client } = this.props;
    let { selectedTabId } = this.state;
    let selectTab = this.selectTab;

    let selectedTab = tabs.find(t => t.id == selectedTabId);

    return React.createElement(
      "div", { className: "app"},
        React.createElement(TabMenu, { tabs, selectedTabId, selectTab }),
        React.createElement("div", { className: "main-content" },
          React.createElement(selectedTab.component, { client }))
        );
  },

  onHashChange() {
    let tabId = window.location.hash.substr(1);

    let isValid = tabs.some(t => t.id == tabId);
    if (isValid) {
      this.setState({ selectedTabId: tabId });
    } else {
      // If the current hash matches no valid category, navigate to the default
      // tab.
      this.selectTab(defaultTabId);
    }
  },

  selectTab(tabId) {
    window.location.hash = "#" + tabId;
  }
});
