/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Sidebar = createFactory(
  require("devtools/client/shared/components/Sidebar")
);

loader.lazyRequireGetter(this, "Menu", "devtools/client/framework/menu");
loader.lazyRequireGetter(
  this,
  "MenuItem",
  "devtools/client/framework/menu-item"
);

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
      allTabsMenuButtonTooltip: PropTypes.string,
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
        // align toggle button to right
        alignRight: PropTypes.bool,
        // if set to true toggle-button rotate 90
        canVerticalSplit: PropTypes.bool,
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
    const { activeTabId, children = [] } = props;
    const tabs = this.createTabs(children);
    const activeTab = tabs.findIndex((tab, index) => tab.id === activeTabId);

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

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { activeTabId, children = [] } = nextProps;
    const tabs = this.createTabs(children);
    const activeTab = tabs.findIndex((tab, index) => tab.id === activeTabId);

    if (
      activeTab !== this.state.activeTab ||
      children !== this.props.children
    ) {
      this.setState({
        activeTab: activeTab === -1 ? 0 : activeTab,
        tabs,
      });
    }
  }

  createTabs(children) {
    return children
      .filter(panel => panel)
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
    const tabs = this.state.tabs.slice();

    if (index >= 0) {
      tabs.splice(index, 0, { id, title, panel, url });
    } else {
      tabs.push({ id, title, panel, url });
    }

    const newState = Object.assign({}, this.state, {
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

    const tabs = this.state.tabs.slice();

    // Preselect the first sidebar tab if none was explicitly selected.
    let activeTab = 0;
    let activeId = this.queuedTabs[0].id;

    for (const { id, index, panel, selected, title, url } of this.queuedTabs) {
      if (index >= 0) {
        tabs.splice(index, 0, { id, title, panel, url });
      } else {
        tabs.push({ id, title, panel, url });
      }

      if (selected) {
        activeId = id;
        activeTab = index >= 0 ? index : tabs.length - 1;
      }
    }

    const newState = Object.assign({}, this.state, {
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
    const index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    const tabs = this.state.tabs.slice();
    tabs[index] = Object.assign({}, tabs[index], {
      isVisible,
    });

    this.setState(
      Object.assign({}, this.state, {
        tabs,
      })
    );
  }

  removeTab(tabId) {
    const index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    const tabs = this.state.tabs.slice();
    tabs.splice(index, 1);

    let activeTab = this.state.activeTab - 1;
    activeTab = activeTab === -1 ? 0 : activeTab;

    this.setState(
      Object.assign({}, this.state, {
        activeTab,
        tabs,
      }),
      () => {
        // Select the next active tab and force the select event handler to initialize
        // the panel if needed.
        if (tabs.length && this.props.onSelect) {
          this.props.onSelect(this.getTabId(activeTab));
        }
      }
    );
  }

  select(tabId) {
    const index = this.getTabIndex(tabId);
    if (index < 0) {
      return;
    }

    const newState = Object.assign({}, this.state, {
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
    this.setState(
      {
        activeTab: index,
      },
      () => {
        if (this.props.onSelect) {
          this.props.onSelect(this.state.tabs[index].id);
        }
      }
    );
  }

  onAllTabsMenuClick(event) {
    const menu = new Menu();
    const target = event.target;

    // Generate list of menu items from the list of tabs.
    this.state.tabs.forEach(tab => {
      menu.append(
        new MenuItem({
          label: tab.title,
          type: "checkbox",
          checked: this.getCurrentTabId() === tab.id,
          click: () => this.select(tab.id),
        })
      );
    });

    // Show a drop down menu with frames.
    menu.popupAtTarget(target);

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
    const tabs = this.state.tabs.map(tab => this.renderTab(tab));

    return div(
      { className: "devtools-sidebar-tabs" },
      Sidebar(
        {
          onAllTabsMenuClick: this.onAllTabsMenuClick,
          renderOnlySelected: this.props.renderOnlySelected,
          showAllTabsMenu: this.props.showAllTabsMenu,
          allTabsMenuButtonTooltip: this.props.allTabsMenuButtonTooltip,
          sidebarToggleButton: this.props.sidebarToggleButton,
          activeTab: this.state.activeTab,
          onAfterChange: this.onTabChanged,
        },
        tabs
      )
    );
  }
}

module.exports = Tabbar;
