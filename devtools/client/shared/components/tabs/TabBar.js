/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

const Sidebar = createFactory(require("devtools/client/shared/components/Sidebar"));

// Shortcuts
const { div } = dom;

/**
 * Renders Tabbar component.
 */
class Tabbar extends Component {
  static get propTypes() {
    return {
      children: PropTypes.array,
      menuDocument: PropTypes.object,
      onSelect: PropTypes.func,
      showAllTabsMenu: PropTypes.bool,
      activeTabId: PropTypes.string,
      renderOnlySelected: PropTypes.bool,
      sidebarToggleButton: PropTypes.shape({
        // Set to true if collapsed.
        collapsed: PropTypes.bool.isRequired,
        // Tooltip text used when the button indicates expanded state.
        collapsePaneTitle: PropTypes.string.isRequired,
        // Tooltip text used when the button indicates collapsed state.
        expandPaneTitle: PropTypes.string.isRequired,
        // Click callback
        onClick: PropTypes.func.isRequired,
      }),
    };
  }

  static get defaultProps() {
    return {
      menuDocument: window.parent.document,
      showAllTabsMenu: false,
    };
  }

  constructor(props, context) {
    super(props, context);
    let { activeTabId, children = [] } = props;
    let tabs = this.createTabs(children);
    let activeTab = tabs.findIndex((tab, index) => tab.id === activeTabId);

    this.state = {
      activeTab: activeTab === -1 ? 0 : activeTab,
      tabs,
    };

    // Array of queued tabs to add to the Tabbar.
    this.queuedTabs = [];

    this.createTabs = this.createTabs.bind(this);
    this.addTab = this.addTab.bind(this);
    this.addAllQueuedTabs = this.addAllQueuedTabs.bind(this);
    this.queueTab = this.queueTab.bind(this);
    this.toggleTab = this.toggleTab.bind(this);
    this.removeTab = this.removeTab.bind(this);
    this.select = this.select.bind(this);
    this.getTabIndex = this.getTabIndex.bind(this);
    this.getTabId = this.getTabId.bind(this);
    this.getCurrentTabId = this.getCurrentTabId.bind(this);
    this.onTabChanged = this.onTabChanged.bind(this);
    this.onAllTabsMenuClick = this.onAllTabsMenuClick.bind(this);
    this.renderTab = this.renderTab.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    let { activeTabId, children = [] } = nextProps;
    let tabs = this.createTabs(children);
    let activeTab = tabs.findIndex((tab, index) => tab.id === activeTabId);

    if (activeTab !== this.state.activeTab ||
        (children !== this.props.children)) {
      this.setState({
        activeTab: activeTab === -1 ? 0 : activeTab,
        tabs,
      });
    }
  }

  createTabs(children) {
    return children
      .filter((panel) => panel)
      .map((panel, index) =>
        Object.assign({}, children[index], {
          id: panel.props.id || index,
          panel,
          title: panel.props.title,
        })
      );
  }

  // Public API

  addTab(id, title, selected = false, panel, url, index = -1) {
    let tabs = this.state.tabs.slice();

    if (index >= 0) {
      tabs.splice(index, 0, {id, title, panel, url});
    } else {
      tabs.push({id, title, panel, url});
    }

    let newState = Object.assign({}, this.state, {
      tabs,
    });

    if (selected) {
      newState.activeTab = index >= 0 ? index : tabs.length - 1;
    }

    this.setState(newState, () => {
      if (this.props.onSelect && selected) {
        this.props.onSelect(id);
      }
    });
  }

  addAllQueuedTabs() {
    if (!this.queuedTabs.length) {
      return;
    }

    let tabs = this.state.tabs.slice();
    let activeId;
    let activeTab;

    for (let { id, index, panel, selected, title, url } of this.queuedTabs) {
      if (index >= 0) {
        tabs.splice(index, 0, {id, title, panel, url});
      } else {
        tabs.push({id, title, panel, url});
      }

      if (selected) {
        activeId = id;
        activeTab = index >= 0 ? index : tabs.length - 1;
      }
    }

    let newState = Object.assign({}, this.state, {
      activeTab,
      tabs,
    });

    this.setState(newState, () => {
      if (this.props.onSelect) {
        this.props.onSelect(activeId);
      }
    });

    this.queuedTabs = [];
  }

  /**
   * Queues a tab to be added. This is more performant than calling addTab for every
   * single tab to be added since we will limit the number of renders happening when
   * a new state is set. Once all the tabs to be added have been queued, call
   * addAllQueuedTabs() to populate the TabBar with all the queued tabs.
   */
  queueTab(id, title, selected = false, panel, url, index = -1) {
    this.queuedTabs.push({
      id,
      index,
      panel,
      selected,
      title,
      url,
    });
  }

  toggleTab(tabId, isVisible) {
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
  }

  removeTab(tabId) {
    let index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    let tabs = this.state.tabs.slice();
    tabs.splice(index, 1);

    let activeTab = this.state.activeTab - 1;
    activeTab = activeTab === -1 ? 0 : activeTab;

    this.setState(Object.assign({}, this.state, {
      activeTab,
      tabs,
    }), () => {
      // Select the next active tab and force the select event handler to initialize
      // the panel if needed.
      if (tabs.length > 0 && this.props.onSelect) {
        this.props.onSelect(this.getTabId(activeTab));
      }
    });
  }

  select(tabId) {
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
  }

  // Helpers

  getTabIndex(tabId) {
    let tabIndex = -1;
    this.state.tabs.forEach((tab, index) => {
      if (tab.id === tabId) {
        tabIndex = index;
      }
    });
    return tabIndex;
  }

  getTabId(index) {
    return this.state.tabs[index].id;
  }

  getCurrentTabId() {
    return this.state.tabs[this.state.activeTab].id;
  }

  // Event Handlers

  onTabChanged(index) {
    this.setState({
      activeTab: index
    }, () => {
      if (this.props.onSelect) {
        this.props.onSelect(this.state.tabs[index].id);
      }
    });
  }

  onAllTabsMenuClick(event) {
    let menu = new Menu();
    let target = event.target;

    // Generate list of menu items from the list of tabs.
    this.state.tabs.forEach((tab) => {
      menu.append(new MenuItem({
        label: tab.title,
        type: "checkbox",
        checked: this.getCurrentTabId() === tab.id,
        click: () => this.select(tab.id),
      }));
    });

    // Show a drop down menu with frames.
    // XXX Missing menu API for specifying target (anchor)
    // and relative position to it. See also:
    // https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/Method/openPopup
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1274551
    let rect = target.getBoundingClientRect();
    let screenX = target.ownerDocument.defaultView.mozInnerScreenX;
    let screenY = target.ownerDocument.defaultView.mozInnerScreenY;
    menu.popupWithZoom(rect.left + screenX, rect.bottom + screenY,
                       { doc: this.props.menuDocument });

    return menu;
  }

  // Rendering

  renderTab(tab) {
    if (typeof tab.panel === "function") {
      return tab.panel({
        key: tab.id,
        title: tab.title,
        id: tab.id,
        url: tab.url,
      });
    }

    return tab.panel;
  }

  render() {
    let tabs = this.state.tabs.map((tab) => this.renderTab(tab));

    return (
      div({className: "devtools-sidebar-tabs"},
        Sidebar({
          onAllTabsMenuClick: this.onAllTabsMenuClick,
          renderOnlySelected: this.props.renderOnlySelected,
          showAllTabsMenu: this.props.showAllTabsMenu,
          sidebarToggleButton: this.props.sidebarToggleButton,
          tabActive: this.state.activeTab,
          onAfterChange: this.onTabChanged,
        },
          tabs
        )
      )
    );
  }
}

module.exports = Tabbar;
