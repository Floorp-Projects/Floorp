/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, createClass, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const Tabs = createFactory(require("devtools/client/shared/components/tabs/tabs").Tabs);

// Shortcuts
const { div } = DOM;

/**
 * Renders Tabbar component.
 */
let Tabbar = createClass({
  displayName: "Tabbar",

  propTypes: {
    onSelect: PropTypes.func,
  },

  getInitialState: function () {
    return {
      tabs: [],
      activeTab: 0
    };
  },

  // Public API

  addTab: function (id, title, selected = false, panel, url) {
    let tabs = this.state.tabs.slice();
    tabs.push({id, title, panel, url});

    let newState = Object.assign({}, this.state, {
      tabs: tabs,
    });

    if (selected) {
      newState.activeTab = tabs.length - 1;
    }

    this.setState(newState);
  },

  toggleTab: function (tabId, isVisible) {
    let index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    let tabs = this.state.tabs.slice();
    tabs[index] = Object.assign({}, tabs[index], {
      isVisible: isVisible
    });

    this.setState(Object.assign({}, this.state, {
      tabs: tabs,
    }));
  },

  removeTab: function (tabId) {
    let index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    let tabs = this.state.tabs.slice();
    tabs.splice(index, 1);

    this.setState(Object.assign({}, this.state, {
      tabs: tabs,
    }));
  },

  select: function (tabId) {
    let index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    let newState = Object.assign({}, this.state, {
      activeTab: index,
    });

    this.setState(newState, () => {
      if (this.props.onSelect) {
        this.props.onSelect(tabId);
      }
    });
  },

  // Helpers

  getTabIndex: function (tabId) {
    let tabIndex = -1;
    this.state.tabs.forEach((tab, index) => {
      if (tab.id == tabId) {
        tabIndex = index;
      }
    });
    return tabIndex;
  },

  getTabId: function (index) {
    return this.state.tabs[index].id;
  },

  getCurrentTabId: function () {
    return this.state.tabs[this.state.activeTab].id;
  },

  // Event Handlers

  onTabChanged: function (index) {
    this.setState({
      activeTab: index
    });

    if (this.props.onSelect) {
      this.props.onSelect(this.state.tabs[index].id);
    }
  },

  // Rendering

  renderTab: function (tab) {
    if (typeof tab.panel === "function") {
      return tab.panel({
        key: tab.id,
        title: tab.title,
        id: tab.id,
        url: tab.url,
      });
    }

    return tab.panel;
  },

  render: function () {
    let tabs = this.state.tabs.map(tab => {
      return this.renderTab(tab);
    });

    return (
      div({className: "devtools-sidebar-tabs"},
        Tabs({
          tabActive: this.state.activeTab,
          onAfterChange: this.onTabChanged},
          tabs
        )
      )
    );
  },
});

module.exports = Tabbar;
