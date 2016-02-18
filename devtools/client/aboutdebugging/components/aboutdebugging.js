/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React */

"use strict";

loader.lazyRequireGetter(this, "React",
  "devtools/client/shared/vendor/react");
loader.lazyRequireGetter(this, "AddonsComponent",
  "devtools/client/aboutdebugging/components/addons", true);
loader.lazyRequireGetter(this, "TabMenuComponent",
  "devtools/client/aboutdebugging/components/tab-menu", true);
loader.lazyRequireGetter(this, "WorkersComponent",
  "devtools/client/aboutdebugging/components/workers", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const tabs = [
  { id: "addons", name: Strings.GetStringFromName("addons"),
    icon: "chrome://devtools/skin/images/debugging-addons.svg",
    component: AddonsComponent },
  { id: "workers", name: Strings.GetStringFromName("workers"),
    icon: "chrome://devtools/skin/images/debugging-workers.svg",
    component: WorkersComponent },
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
    this.props.window.addEventListener("hashchange", this.onHashChange);
    this.onHashChange();
    this.props.telemetry.toolOpened("aboutdebugging");
  },

  componentWillUnmount() {
    this.props.window.removeEventListener("hashchange", this.onHashChange);
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
        React.createElement(TabMenuComponent,
                              { tabs, selectedTabId, selectTab }),
        React.createElement("div", { className: "main-content" },
          React.createElement(selectedTab.component, { client }))
        );
  },

  onHashChange() {
    let tabId = this.props.window.location.hash.substr(1);

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
    let win = this.props.window;
    win.location.hash = "#" + tabId;
  }
});
