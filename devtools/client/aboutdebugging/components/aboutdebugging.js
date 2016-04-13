/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals AddonsTab, WorkersTab */

"use strict";

const { createFactory, createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const TabMenu = createFactory(require("./tab-menu"));

loader.lazyGetter(this, "AddonsTab",
  () => createFactory(require("./addons-tab")));
loader.lazyGetter(this, "WorkersTab",
  () => createFactory(require("./workers-tab")));

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const tabs = [{
  id: "addons",
  name: Strings.GetStringFromName("addons"),
  icon: "chrome://devtools/skin/images/debugging-addons.svg",
  component: AddonsTab
}, {
  id: "workers",
  name: Strings.GetStringFromName("workers"),
  icon: "chrome://devtools/skin/images/debugging-workers.svg",
  component: WorkersTab
}];

const defaultTabId = "addons";

module.exports = createClass({
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
  },

  render() {
    let { client } = this.props;
    let { selectedTabId } = this.state;
    let selectTab = this.selectTab;

    let selectedTab = tabs.find(t => t.id == selectedTabId);

    return dom.div({ className: "app" },
      TabMenu({ tabs, selectedTabId, selectTab }),
      dom.div({ className: "main-content" },
        selectedTab.component({ client })
      )
    );
  }
});
