/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const React = require("devtools/client/shared/vendor/react");
  const { Component, DOM } = React;
  const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");

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
        className: React.PropTypes.oneOfType([
          React.PropTypes.array,
          React.PropTypes.string,
          React.PropTypes.object
        ]),
        tabActive: React.PropTypes.number,
        onMount: React.PropTypes.func,
        onBeforeChange: React.PropTypes.func,
        onAfterChange: React.PropTypes.func,
        children: React.PropTypes.oneOfType([
          React.PropTypes.array,
          React.PropTypes.element
        ]).isRequired,
        showAllTabsMenu: React.PropTypes.bool,
        onAllTabsMenuClick: React.PropTypes.func,

        // Set true will only render selected panel on DOM. It's complete
        // opposite of the created array, and it's useful if panels content
        // is unpredictable and update frequently.
        renderOnlySelected: React.PropTypes.bool,
      };
    }

    static get defaultProps() {
      return {
        tabActive: 0,
        showAllTabsMenu: false,
        renderOnlySelected: false,
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        tabActive: props.tabActive,

        // This array is used to store an information whether a tab
        // at specific index has already been created (e.g. selected
        // at least once).
        // If yes, it's rendered even if not currently selected.
        // This is because in some cases we don't want to re-create
        // tab content when it's being unselected/selected.
        // E.g. in case of an iframe being used as a tab-content
        // we want the iframe to stay in the DOM.
        created: [],

        // True if tabs can't fit into available horizontal space.
        overflow: false,
      };

      this.onOverflow = this.onOverflow.bind(this);
      this.onUnderflow = this.onUnderflow.bind(this);
      this.onKeyDown = this.onKeyDown.bind(this);
      this.onClickTab = this.onClickTab.bind(this);
      this.setActive = this.setActive.bind(this);
      this.renderMenuItems = this.renderMenuItems.bind(this);
      this.renderPanels = this.renderPanels.bind(this);
    }

    componentDidMount() {
      let node = findDOMNode(this);
      node.addEventListener("keydown", this.onKeyDown);

      // Register overflow listeners to manage visibility
      // of all-tabs-menu. This menu is displayed when there
      // is not enough h-space to render all tabs.
      // It allows the user to select a tab even if it's hidden.
      if (this.props.showAllTabsMenu) {
        node.addEventListener("overflow", this.onOverflow);
        node.addEventListener("underflow", this.onUnderflow);
      }

      let index = this.state.tabActive;
      if (this.props.onMount) {
        this.props.onMount(index);
      }
    }

    componentWillReceiveProps(nextProps) {
      let { children, tabActive } = nextProps;

      // Check type of 'tabActive' props to see if it's valid
      // (it's 0-based index).
      if (typeof tabActive === "number") {
        let panels = children.filter((panel) => panel);

        // Reset to index 0 if index overflows the range of panel array
        tabActive = (tabActive < panels.length && tabActive >= 0) ?
          tabActive : 0;

        let created = [...this.state.created];
        created[tabActive] = true;

        this.setState({
          created,
          tabActive,
        });
      }
    }

    componentWillUnmount() {
      let node = findDOMNode(this);
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
          overflow: true
        });
      }
    }

    onUnderflow(event) {
      if (event.target.classList.contains("tabs-menu")) {
        this.setState({
          overflow: false
        });
      }
    }

    onKeyDown(event) {
      // Bail out if the focus isn't on a tab.
      if (!event.target.closest(".tabs-menu-item")) {
        return;
      }

      let tabActive = this.state.tabActive;
      let tabCount = this.props.children.length;

      switch (event.code) {
        case "ArrowRight":
          tabActive = Math.min(tabCount - 1, tabActive + 1);
          break;
        case "ArrowLeft":
          tabActive = Math.max(0, tabActive - 1);
          break;
      }

      if (this.state.tabActive != tabActive) {
        this.setActive(tabActive);
      }
    }

    onClickTab(index, event) {
      this.setActive(index);

      if (event) {
        event.preventDefault();
      }
    }

    // API

    setActive(index) {
      let onAfterChange = this.props.onAfterChange;
      let onBeforeChange = this.props.onBeforeChange;

      if (onBeforeChange) {
        let cancel = onBeforeChange(index);
        if (cancel) {
          return;
        }
      }

      let created = [...this.state.created];
      created[index] = true;

      let newState = Object.assign({}, this.state, {
        tabActive: index,
        created: created
      });

      this.setState(newState, () => {
        // Properly set focus on selected tab.
        let node = findDOMNode(this);
        let selectedTab = node.querySelector(".is-active > a");
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

      let tabs = this.props.children
        .map((tab) => typeof tab === "function" ? tab() : tab)
        .filter((tab) => tab)
        .map((tab, index) => {
          let {
            id,
            className: tabClassName,
            title,
            badge,
            showBadge,
          } = tab.props;

          let ref = "tab-menu-" + index;
          let isTabSelected = this.state.tabActive === index;

          let className = [
            "tabs-menu-item",
            tabClassName,
            isTabSelected ? "is-active" : "",
          ].join(" ");

          // Set tabindex to -1 (except the selected tab) so, it's focusable,
          // but not reachable via sequential tab-key navigation.
          // Changing selected tab (and so, moving focus) is done through
          // left and right arrow keys.
          // See also `onKeyDown()` event handler.
          return (
            DOM.li({
              className,
              key: index,
              ref,
              role: "presentation",
            },
              DOM.span({className: "devtools-tab-line"}),
              DOM.a({
                id: id ? id + "-tab" : "tab-" + index,
                tabIndex: isTabSelected ? 0 : -1,
                "aria-controls": id ? id + "-panel" : "panel-" + index,
                "aria-selected": isTabSelected,
                role: "tab",
                onClick: this.onClickTab.bind(this, index),
              },
                title,
                badge && !isTabSelected && showBadge() ?
                  DOM.span({ className: "tab-badge" }, badge)
                  :
                  null
              )
            )
          );
        });

      // Display the menu only if there is not enough horizontal
      // space for all tabs (and overflow happened).
      let allTabsMenu = this.state.overflow ? (
        DOM.div({
          className: "all-tabs-menu",
          onClick: this.props.onAllTabsMenuClick,
        })
      ) : null;

      return (
        DOM.nav({className: "tabs-navigation"},
          DOM.ul({className: "tabs-menu", role: "tablist"},
            tabs
          ),
          allTabsMenu
        )
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

      let selectedIndex = this.state.tabActive;

      let panels = children
        .map((tab) => typeof tab === "function" ? tab() : tab)
        .filter((tab) => tab)
        .map((tab, index) => {
          let selected = selectedIndex === index;
          if (renderOnlySelected && !selected) {
            return null;
          }

          let id = tab.props.id;

          // Use 'visibility:hidden' + 'height:0' for hiding content of non-selected
          // tab. It's faster than 'display:none' because it avoids triggering frame
          // destruction and reconstruction. 'width' is not changed to avoid relayout.
          let style = {
            visibility: selected ? "visible" : "hidden",
            height: selected ? "100%" : "0",
          };

          // Allows lazy loading panels by creating them only if they are selected,
          // then store a copy of the lazy created panel in `tab.panel`.
          if (typeof tab.panel == "function" && selected) {
            tab.panel = tab.panel(tab);
          }
          let panel = tab.panel || tab;

          return (
            DOM.div({
              id: id ? id + "-panel" : "panel-" + index,
              key: index,
              style: style,
              className: selected ? "tab-panel-box" : "tab-panel-box hidden",
              role: "tabpanel",
              "aria-labelledby": id ? id + "-tab" : "tab-" + index,
            },
              (selected || this.state.created[index]) ? panel : null
            )
          );
        });

      return (
        DOM.div({className: "panels"},
          panels
        )
      );
    }

    render() {
      return (
        DOM.div({ className: ["tabs", this.props.className].join(" ") },
          this.renderMenuItems(),
          this.renderPanels()
        )
      );
    }
  }

  /**
   * Renders simple tab 'panel'.
   */
  class Panel extends Component {
    static get propTypes() {
      return {
        title: React.PropTypes.string.isRequired,
        children: React.PropTypes.oneOfType([
          React.PropTypes.array,
          React.PropTypes.element
        ]).isRequired
      };
    }

    render() {
      return DOM.div({className: "tab-panel"},
        this.props.children
      );
    }
  }

  // Exports from this module
  exports.TabPanel = Panel;
  exports.Tabs = Tabs;
});
