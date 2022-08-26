/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
  createRef,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  ToolboxTabsOrderManager,
} = require("devtools/client/framework/toolbox-tabs-order-manager");

const { div } = dom;

const ToolboxTab = createFactory(
  require("devtools/client/framework/components/ToolboxTab")
);

loader.lazyGetter(this, "MenuButton", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuButton")
  );
});
loader.lazyGetter(this, "MenuItem", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuItem")
  );
});
loader.lazyGetter(this, "MenuList", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuList")
  );
});

// 26px is chevron devtools button width.(i.e. tools-chevronmenu)
const CHEVRON_BUTTON_WIDTH = 26;

class ToolboxTabs extends Component {
  // See toolbox-toolbar propTypes for details on the props used here.
  static get propTypes() {
    return {
      currentToolId: PropTypes.string,
      focusButton: PropTypes.func,
      focusedButton: PropTypes.string,
      highlightedTools: PropTypes.object,
      panelDefinitions: PropTypes.array,
      selectTool: PropTypes.func,
      toolbox: PropTypes.object,
      visibleToolboxButtonCount: PropTypes.number.isRequired,
      L10N: PropTypes.object,
      onTabsOrderUpdated: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Array of overflowed tool id.
      overflowedTabIds: [],
    };

    this.wrapperEl = createRef();

    // Map with tool Id and its width size. This lifecycle is out of React's
    // lifecycle. If a tool is registered, ToolboxTabs will add target tool id
    // to this map. ToolboxTabs will never remove tool id from this cache.
    this._cachedToolTabsWidthMap = new Map();

    this._resizeTimerId = null;
    this.resizeHandler = this.resizeHandler.bind(this);

    const { toolbox, onTabsOrderUpdated, panelDefinitions } = props;
    this._tabsOrderManager = new ToolboxTabsOrderManager(
      toolbox,
      onTabsOrderUpdated,
      panelDefinitions
    );
  }

  componentDidMount() {
    window.addEventListener("resize", this.resizeHandler);
    this.updateCachedToolTabsWidthMap();
    this.updateOverflowedTabs();
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillUpdate(nextProps, nextState) {
    if (this.shouldUpdateToolboxTabs(this.props, nextProps)) {
      // Force recalculate and render in this cycle if panel definition has
      // changed or selected tool has changed.
      nextState.overflowedTabIds = [];
    }
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.shouldUpdateToolboxTabs(prevProps, this.props)) {
      this.updateCachedToolTabsWidthMap();
      this.updateOverflowedTabs();
      this._tabsOrderManager.setCurrentPanelDefinitions(
        this.props.panelDefinitions
      );
    }
  }

  componentWillUnmount() {
    window.removeEventListener("resize", this.resizeHandler);
    window.cancelIdleCallback(this._resizeTimerId);
    this._tabsOrderManager.destroy();
  }

  /**
   * Check if two array of ids are the same or not.
   */
  equalToolIdArray(prevPanels, nextPanels) {
    if (prevPanels.length !== nextPanels.length) {
      return false;
    }

    // Check panel definitions even if both of array size is same.
    // For example, the case of changing the tab's order.
    return prevPanels.join("-") === nextPanels.join("-");
  }

  /**
   * Return true if we should update the overflowed tabs.
   */
  shouldUpdateToolboxTabs(prevProps, nextProps) {
    if (
      prevProps.currentToolId !== nextProps.currentToolId ||
      prevProps.visibleToolboxButtonCount !==
        nextProps.visibleToolboxButtonCount
    ) {
      return true;
    }

    const prevPanels = prevProps.panelDefinitions.map(def => def.id);
    const nextPanels = nextProps.panelDefinitions.map(def => def.id);
    return !this.equalToolIdArray(prevPanels, nextPanels);
  }

  /**
   * Update the Map of tool id and tool tab width.
   */
  updateCachedToolTabsWidthMap() {
    const utils = window.windowUtils;
    // Force a reflow before calling getBoundingWithoutFlushing on each tab.
    this.wrapperEl.current.clientWidth;

    for (const tab of this.wrapperEl.current.querySelectorAll(
      ".devtools-tab"
    )) {
      const tabId = tab.id.replace("toolbox-tab-", "");
      if (!this._cachedToolTabsWidthMap.has(tabId)) {
        const rect = utils.getBoundsWithoutFlushing(tab);
        this._cachedToolTabsWidthMap.set(tabId, rect.width);
      }
    }
  }

  /**
   * Update the overflowed tab array from currently displayed tool tab.
   * If calculated result is the same as the current overflowed tab array, this
   * function will not update state.
   */
  updateOverflowedTabs() {
    const toolboxWidth = parseInt(
      getComputedStyle(this.wrapperEl.current).width,
      10
    );
    const { currentToolId } = this.props;
    const enabledTabs = this.props.panelDefinitions.map(def => def.id);
    let sumWidth = 0;
    const visibleTabs = [];

    for (const id of enabledTabs) {
      const width = this._cachedToolTabsWidthMap.get(id);
      sumWidth += width;
      if (sumWidth <= toolboxWidth) {
        visibleTabs.push(id);
      } else {
        sumWidth = sumWidth - width + CHEVRON_BUTTON_WIDTH;

        // If toolbox can't display the Chevron, remove the last tool tab.
        if (sumWidth > toolboxWidth) {
          const removeTabId = visibleTabs.pop();
          sumWidth -= this._cachedToolTabsWidthMap.get(removeTabId);
        }
        break;
      }
    }

    // If the selected tab is in overflowed tabs, insert it into a visible
    // toolbox.
    if (
      !visibleTabs.includes(currentToolId) &&
      enabledTabs.includes(currentToolId)
    ) {
      const selectedToolWidth = this._cachedToolTabsWidthMap.get(currentToolId);
      while (
        sumWidth + selectedToolWidth > toolboxWidth &&
        visibleTabs.length
      ) {
        const removingToolId = visibleTabs.pop();
        const removingToolWidth = this._cachedToolTabsWidthMap.get(
          removingToolId
        );
        sumWidth -= removingToolWidth;
      }

      // If toolbox width is narrow, toolbox display only chevron menu.
      // i.e. All tool tabs will overflow.
      if (sumWidth + selectedToolWidth <= toolboxWidth) {
        visibleTabs.push(currentToolId);
      }
    }

    const willOverflowTabs = enabledTabs.filter(
      id => !visibleTabs.includes(id)
    );
    if (!this.equalToolIdArray(this.state.overflowedTabIds, willOverflowTabs)) {
      this.setState({ overflowedTabIds: willOverflowTabs });
    }
  }

  resizeHandler(evt) {
    window.cancelIdleCallback(this._resizeTimerId);
    this._resizeTimerId = window.requestIdleCallback(
      () => {
        this.updateOverflowedTabs();
      },
      { timeout: 100 }
    );
  }

  renderToolsChevronMenuList() {
    const { panelDefinitions, selectTool } = this.props;

    const items = [];
    for (const { id, label, icon } of panelDefinitions) {
      if (this.state.overflowedTabIds.includes(id)) {
        items.push(
          MenuItem({
            key: id,
            id: "tools-chevron-menupopup-" + id,
            label,
            type: "checkbox",
            onClick: () => {
              selectTool(id, "tab_switch");
            },
            icon,
          })
        );
      }
    }

    return MenuList({ id: "tools-chevron-menupopup" }, items);
  }

  /**
   * Render a button to access overflowed tools, displayed only when the toolbar
   * presents an overflow.
   */
  renderToolsChevronButton() {
    const { toolbox } = this.props;

    return MenuButton(
      {
        id: "tools-chevron-menu-button",
        menuId: "tools-chevron-menu-button-panel",
        className: "devtools-tabbar-button tools-chevron-menu",
        toolboxDoc: toolbox.doc,
      },
      this.renderToolsChevronMenuList()
    );
  }

  /**
   * Render all of the tabs, based on the panel definitions and builds out
   * a toolbox tab for each of them. Will render the chevron button if the
   * container has an overflow.
   */
  render() {
    const {
      currentToolId,
      focusButton,
      focusedButton,
      highlightedTools,
      panelDefinitions,
      selectTool,
    } = this.props;

    const tabs = panelDefinitions.map(panelDefinition => {
      // Don't display overflowed tab.
      if (!this.state.overflowedTabIds.includes(panelDefinition.id)) {
        return ToolboxTab({
          key: panelDefinition.id,
          currentToolId,
          focusButton,
          focusedButton,
          highlightedTools,
          panelDefinition,
          selectTool,
        });
      }
      return null;
    });

    return div(
      {
        className: "toolbox-tabs-wrapper",
        ref: this.wrapperEl,
      },
      div(
        {
          className: "toolbox-tabs",
          onMouseDown: e => this._tabsOrderManager.onMouseDown(e),
        },
        tabs,
        this.state.overflowedTabIds.length
          ? this.renderToolsChevronButton()
          : null
      )
    );
  }
}

module.exports = ToolboxTabs;
