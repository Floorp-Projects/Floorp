/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {findDOMNode} = require("devtools/client/shared/vendor/react-dom");
const {button, div} = dom;

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");
const ToolboxTab = createFactory(require("devtools/client/framework/components/toolbox-tab"));
const { ToolboxTabsOrderManager } = require("devtools/client/framework/toolbox-tabs-order-manager");

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

    // Map with tool Id and its width size. This lifecycle is out of React's
    // lifecycle. If a tool is registered, ToolboxTabs will add target tool id
    // to this map. ToolboxTabs will never remove tool id from this cache.
    this._cachedToolTabsWidthMap = new Map();

    this._resizeTimerId = null;
    this.resizeHandler = this.resizeHandler.bind(this);

    this._tabsOrderManager = new ToolboxTabsOrderManager(props.onTabsOrderUpdated);
  }

  componentDidMount() {
    window.addEventListener("resize", this.resizeHandler);
    this.updateCachedToolTabsWidthMap();
    this.updateOverflowedTabs();
  }

  componentWillUpdate(nextProps, nextState) {
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
    }
  }

  componentWillUnmount() {
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
    if (prevProps.currentToolId !== nextProps.currentToolId) {
      return true;
    }

    let prevPanels = prevProps.panelDefinitions.map(def => def.id);
    let nextPanels = nextProps.panelDefinitions.map(def => def.id);
    return !this.equalToolIdArray(prevPanels, nextPanels);
  }

  /**
   * Update the Map of tool id and tool tab width.
   */
  updateCachedToolTabsWidthMap() {
    let thisNode = findDOMNode(this);
    for (let tab of thisNode.querySelectorAll(".devtools-tab")) {
      let tabId = tab.id.replace("toolbox-tab-", "");
      if (!this._cachedToolTabsWidthMap.has(tabId)) {
        let cs = getComputedStyle(tab);
        this._cachedToolTabsWidthMap.set(tabId, parseInt(cs.width, 10));
      }
    }
  }

  /**
   * Update the overflowed tab array from currently displayed tool tab.
   * If calculated result is the same as the current overflowed tab array, this
   * function will not update state.
   */
  updateOverflowedTabs() {
    let node = findDOMNode(this);
    const toolboxWidth = parseInt(getComputedStyle(node).width, 10);
    let { currentToolId } = this.props;
    let enabledTabs = this.props.panelDefinitions.map(def => def.id);
    let sumWidth = 0;
    let visibleTabs = [];

    for (const id of enabledTabs) {
      let width = this._cachedToolTabsWidthMap.get(id);
      sumWidth += width;
      if (sumWidth <= toolboxWidth) {
        visibleTabs.push(id);
      } else {
        sumWidth = sumWidth - width + CHEVRON_BUTTON_WIDTH;

        // If toolbox can't display the Chevron, remove the last tool tab.
        if (sumWidth > toolboxWidth) {
          let removeTabId = visibleTabs.pop();
          sumWidth -= this._cachedToolTabsWidthMap.get(removeTabId);
        }
        break;
      }
    }

    // If the selected tab is in overflowed tabs, insert it into a visible
    // toolbox.
    if (!visibleTabs.includes(currentToolId) &&
        enabledTabs.includes(currentToolId)) {
      let selectedToolWidth = this._cachedToolTabsWidthMap.get(currentToolId);
      while ((sumWidth + selectedToolWidth) > toolboxWidth &&
             visibleTabs.length > 0) {
        let removingToolId  = visibleTabs.pop();
        let removingToolWidth = this._cachedToolTabsWidthMap.get(removingToolId);
        sumWidth -= removingToolWidth;
      }
      visibleTabs.push(currentToolId);
    }

    if (visibleTabs.length === 0) {
      visibleTabs = [enabledTabs[0]];
    }

    let willOverflowTabs = enabledTabs.filter(id => !visibleTabs.includes(id));
    if (!this.equalToolIdArray(this.state.overflowedTabIds, willOverflowTabs)) {
      this.setState({ overflowedTabIds: willOverflowTabs });
    }
  }

  resizeHandler(evt) {
    window.cancelIdleCallback(this._resizeTimerId);
    this._resizeTimerId = window.requestIdleCallback(() => {
      this.updateOverflowedTabs();
    }, { timeout: 300 });
  }

  /**
   * Render a button to access overflowed tools, displayed only when the toolbar
   * presents an overflow.
   */
  renderToolsChevronButton() {
    let {
      panelDefinitions,
      selectTool,
      toolbox,
      L10N,
    } = this.props;

    return button({
      className: "devtools-button tools-chevron-menu",
      tabIndex: -1,
      title: L10N.getStr("toolbox.allToolsButton.tooltip"),
      id: "tools-chevron-menu-button",
      onClick: ({ target }) => {
        let menu = new Menu({
          id: "tools-chevron-menupopup"
        });

        panelDefinitions.forEach(({id, label}) => {
          if (this.state.overflowedTabIds.includes(id)) {
            menu.append(new MenuItem({
              click: () => {
                selectTool(id);
              },
              id: "tools-chevron-menupopup-" + id,
              label,
              type: "checkbox",
            }));
          }
        });

        let rect = target.getBoundingClientRect();
        let screenX = target.ownerDocument.defaultView.mozInnerScreenX;
        let screenY = target.ownerDocument.defaultView.mozInnerScreenY;

        // Display the popup below the button.
        menu.popup(rect.left + screenX, rect.bottom + screenY, toolbox);
        return menu;
      },
    });
  }

  /**
   * Render all of the tabs, based on the panel definitions and builds out
   * a toolbox tab for each of them. Will render the chevron button if the
   * container has an overflow.
   */
  render() {
    let {
      currentToolId,
      focusButton,
      focusedButton,
      highlightedTools,
      panelDefinitions,
      selectTool,
    } = this.props;

    this._tabsOrderManager.setOverflowedTabs(this.state.overflowedTabIds);

    let tabs = panelDefinitions.map(panelDefinition => {
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
        className: "toolbox-tabs-wrapper"
      },
      div(
        {
          className: "toolbox-tabs",
          onMouseDown: (e) => this._tabsOrderManager.onMouseDown(e),
        },
        tabs,
        (this.state.overflowedTabIds.length > 0)
          ? this.renderToolsChevronButton() : null
      )
    );
  }
}

module.exports = ToolboxTabs;
