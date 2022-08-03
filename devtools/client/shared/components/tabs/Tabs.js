/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const {
    Component,
    createRef,
  } = require("devtools/client/shared/vendor/react");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

  /**
   * Renders simple 'tab' widget.
   *
   * Based on ReactSimpleTabs component
   * https://github.com/pedronauck/react-simpletabs
   *
   * Component markup (+CSS) example:
   *
   * <div class='tabs'>
   *  <nav class='tabs-navigation'>
   *    <ul class='tabs-menu'>
   *      <li class='tabs-menu-item is-active'>Tab #1</li>
   *      <li class='tabs-menu-item'>Tab #2</li>
   *    </ul>
   *  </nav>
   *  <div class='panels'>
   *    The content of active panel here
   *  </div>
   * <div>
   */
  class Tabs extends Component {
    static get propTypes() {
      return {
        className: PropTypes.oneOfType([
          PropTypes.array,
          PropTypes.string,
          PropTypes.object,
        ]),
        activeTab: PropTypes.number,
        onMount: PropTypes.func,
        onBeforeChange: PropTypes.func,
        onAfterChange: PropTypes.func,
        children: PropTypes.oneOfType([PropTypes.array, PropTypes.element])
          .isRequired,
        showAllTabsMenu: PropTypes.bool,
        allTabsMenuButtonTooltip: PropTypes.string,
        onAllTabsMenuClick: PropTypes.func,

        // To render a sidebar toggle button before the tab menu provide a function that
        // returns a React component for the button.
        renderSidebarToggle: PropTypes.func,
        // Set true will only render selected panel on DOM. It's complete
        // opposite of the created array, and it's useful if panels content
        // is unpredictable and update frequently.
        renderOnlySelected: PropTypes.bool,
      };
    }

    static get defaultProps() {
      return {
        activeTab: 0,
        showAllTabsMenu: false,
        renderOnlySelected: false,
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        activeTab: props.activeTab,

        // This array is used to store an object containing information on whether a tab
        // at a specified index has already been created (e.g. selected at least once) and
        // the tab id. An example of the object structure is the following:
        // [{ isCreated: true, tabId: "ruleview" }, { isCreated: false, tabId: "foo" }].
        // If the tab at the specified index has already been created, it's rendered even
        // if not currently selected. This is because in some cases we don't want
        // to re-create tab content when it's being unselected/selected.
        // E.g. in case of an iframe being used as a tab-content we want the iframe to
        // stay in the DOM.
        created: [],

        // True if tabs can't fit into available horizontal space.
        overflow: false,
      };

      this.tabsEl = createRef();

      this.onOverflow = this.onOverflow.bind(this);
      this.onUnderflow = this.onUnderflow.bind(this);
      this.onKeyDown = this.onKeyDown.bind(this);
      this.onClickTab = this.onClickTab.bind(this);
      this.setActive = this.setActive.bind(this);
      this.renderMenuItems = this.renderMenuItems.bind(this);
      this.renderPanels = this.renderPanels.bind(this);
    }

    componentDidMount() {
      const node = this.tabsEl.current;
      node.addEventListener("keydown", this.onKeyDown);

      // Register overflow listeners to manage visibility
      // of all-tabs-menu. This menu is displayed when there
      // is not enough h-space to render all tabs.
      // It allows the user to select a tab even if it's hidden.
      if (this.props.showAllTabsMenu) {
        node.addEventListener("overflow", this.onOverflow);
        node.addEventListener("underflow", this.onUnderflow);
      }

      const index = this.state.activeTab;
      if (this.props.onMount) {
        this.props.onMount(index);
      }
    }

    // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
    UNSAFE_componentWillReceiveProps(nextProps) {
      let { children, activeTab } = nextProps;
      const panels = children.filter(panel => panel);
      let created = [...this.state.created];

      // If the children props has changed due to an addition or removal of a tab,
      // update the state's created array with the latest tab ids and whether or not
      // the tab is already created.
      if (this.state.created.length != panels.length) {
        created = panels.map(panel => {
          // Get whether or not the tab has already been created from the previous state.
          const createdEntry = this.state.created.find(entry => {
            return entry && entry.tabId === panel.props.id;
          });
          const isCreated = !!createdEntry && createdEntry.isCreated;
          const tabId = panel.props.id;

          return {
            isCreated,
            tabId,
          };
        });
      }

      // Check type of 'activeTab' props to see if it's valid (it's 0-based index).
      if (typeof activeTab === "number") {
        // Reset to index 0 if index overflows the range of panel array
        activeTab = activeTab < panels.length && activeTab >= 0 ? activeTab : 0;

        created[activeTab] = Object.assign({}, created[activeTab], {
          isCreated: true,
        });

        this.setState({
          activeTab,
        });
      }

      this.setState({
        created,
      });
    }

    componentWillUnmount() {
      const node = this.tabsEl.current;
      node.removeEventListener("keydown", this.onKeyDown);

      if (this.props.showAllTabsMenu) {
        node.removeEventListener("overflow", this.onOverflow);
        node.removeEventListener("underflow", this.onUnderflow);
      }
    }

    // DOM Events

    onOverflow(event) {
      if (event.target.classList.contains("tabs-menu")) {
        this.setState({
          overflow: true,
        });
      }
    }

    onUnderflow(event) {
      if (event.target.classList.contains("tabs-menu")) {
        this.setState({
          overflow: false,
        });
      }
    }

    onKeyDown(event) {
      // Bail out if the focus isn't on a tab.
      if (!event.target.closest(".tabs-menu-item")) {
        return;
      }

      let activeTab = this.state.activeTab;
      const tabCount = this.props.children.length;

      const ltr = event.target.ownerDocument.dir == "ltr";
      const nextOrLastTab = Math.min(tabCount - 1, activeTab + 1);
      const previousOrFirstTab = Math.max(0, activeTab - 1);

      switch (event.code) {
        case "ArrowRight":
          if (ltr) {
            activeTab = nextOrLastTab;
          } else {
            activeTab = previousOrFirstTab;
          }
          break;
        case "ArrowLeft":
          if (ltr) {
            activeTab = previousOrFirstTab;
          } else {
            activeTab = nextOrLastTab;
          }
          break;
      }

      if (this.state.activeTab != activeTab) {
        this.setActive(activeTab);
      }
    }

    onClickTab(index, event) {
      this.setActive(index);

      if (event) {
        event.preventDefault();
      }
    }

    onMouseDown(event) {
      // Prevents click-dragging the tab headers
      if (event) {
        event.preventDefault();
      }
    }

    // API

    setActive(index) {
      const onAfterChange = this.props.onAfterChange;
      const onBeforeChange = this.props.onBeforeChange;

      if (onBeforeChange) {
        const cancel = onBeforeChange(index);
        if (cancel) {
          return;
        }
      }

      const created = [...this.state.created];
      created[index] = Object.assign({}, created[index], {
        isCreated: true,
      });

      const newState = Object.assign({}, this.state, {
        created,
        activeTab: index,
      });

      this.setState(newState, () => {
        // Properly set focus on selected tab.
        const selectedTab = this.tabsEl.current.querySelector(".is-active > a");
        if (selectedTab) {
          selectedTab.focus();
        }

        if (onAfterChange) {
          onAfterChange(index);
        }
      });
    }

    // Rendering

    renderMenuItems() {
      if (!this.props.children) {
        throw new Error("There must be at least one Tab");
      }

      if (!Array.isArray(this.props.children)) {
        this.props.children = [this.props.children];
      }

      const tabs = this.props.children
        .map(tab => (typeof tab === "function" ? tab() : tab))
        .filter(tab => tab)
        .map((tab, index) => {
          const {
            id,
            className: tabClassName,
            title,
            badge,
            showBadge,
          } = tab.props;

          const ref = "tab-menu-" + index;
          const isTabSelected = this.state.activeTab === index;

          const className = [
            "tabs-menu-item",
            tabClassName,
            isTabSelected ? "is-active" : "",
          ].join(" ");

          // Set tabindex to -1 (except the selected tab) so, it's focusable,
          // but not reachable via sequential tab-key navigation.
          // Changing selected tab (and so, moving focus) is done through
          // left and right arrow keys.
          // See also `onKeyDown()` event handler.
          return dom.li(
            {
              className,
              key: index,
              ref,
              role: "presentation",
            },
            dom.span({ className: "devtools-tab-line" }),
            dom.a(
              {
                id: id ? id + "-tab" : "tab-" + index,
                tabIndex: isTabSelected ? 0 : -1,
                title,
                "aria-controls": id ? id + "-panel" : "panel-" + index,
                "aria-selected": isTabSelected,
                role: "tab",
                onClick: this.onClickTab.bind(this, index),
                onMouseDown: this.onMouseDown.bind(this),
              },
              title,
              badge && !isTabSelected && showBadge()
                ? dom.span({ className: "tab-badge" }, badge)
                : null
            )
          );
        });

      // Display the menu only if there is not enough horizontal
      // space for all tabs (and overflow happened).
      const allTabsMenu = this.state.overflow
        ? dom.button({
            className: "all-tabs-menu",
            title: this.props.allTabsMenuButtonTooltip,
            onClick: this.props.onAllTabsMenuClick,
          })
        : null;

      // Get the sidebar toggle button if a renderSidebarToggle function is provided.
      const sidebarToggle = this.props.renderSidebarToggle
        ? this.props.renderSidebarToggle()
        : null;

      return dom.nav(
        { className: "tabs-navigation" },
        sidebarToggle,
        dom.ul({ className: "tabs-menu", role: "tablist" }, tabs),
        allTabsMenu
      );
    }

    renderPanels() {
      let { children, renderOnlySelected } = this.props;

      if (!children) {
        throw new Error("There must be at least one Tab");
      }

      if (!Array.isArray(children)) {
        children = [children];
      }

      const selectedIndex = this.state.activeTab;

      const panels = children
        .map(tab => (typeof tab === "function" ? tab() : tab))
        .filter(tab => tab)
        .map((tab, index) => {
          const selected = selectedIndex === index;
          if (renderOnlySelected && !selected) {
            return null;
          }

          const id = tab.props.id;
          const isCreated =
            this.state.created[index] && this.state.created[index].isCreated;

          // Use 'visibility:hidden' + 'height:0' for hiding content of non-selected
          // tab. It's faster than 'display:none' because it avoids triggering frame
          // destruction and reconstruction. 'width' is not changed to avoid relayout.
          const style = {
            visibility: selected ? "visible" : "hidden",
            height: selected ? "100%" : "0",
          };

          // Allows lazy loading panels by creating them only if they are selected,
          // then store a copy of the lazy created panel in `tab.panel`.
          if (typeof tab.panel == "function" && selected) {
            tab.panel = tab.panel(tab);
          }
          const panel = tab.panel || tab;

          return dom.div(
            {
              id: id ? id + "-panel" : "panel-" + index,
              key: id,
              style,
              className: selected ? "tab-panel-box" : "tab-panel-box hidden",
              role: "tabpanel",
              "aria-labelledby": id ? id + "-tab" : "tab-" + index,
            },
            selected || isCreated ? panel : null
          );
        });

      return dom.div({ className: "panels" }, panels);
    }

    render() {
      return dom.div(
        {
          className: ["tabs", this.props.className].join(" "),
          ref: this.tabsEl,
        },
        this.renderMenuItems(),
        this.renderPanels()
      );
    }
  }

  /**
   * Renders simple tab 'panel'.
   */
  class Panel extends Component {
    static get propTypes() {
      return {
        id: PropTypes.string.isRequired,
        className: PropTypes.string,
        title: PropTypes.string.isRequired,
        children: PropTypes.oneOfType([PropTypes.array, PropTypes.element])
          .isRequired,
      };
    }

    render() {
      const { className } = this.props;
      return dom.div(
        { className: `tab-panel ${className || ""}` },
        this.props.children
      );
    }
  }

  // Exports from this module
  exports.TabPanel = Panel;
  exports.Tabs = Tabs;
});
