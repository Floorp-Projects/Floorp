/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const React = require("devtools/client/shared/vendor/react");
  const { DOM } = React;
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
  let Tabs = React.createClass({
    displayName: "Tabs",

    propTypes: {
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
      ]).isRequired
    },

    getDefaultProps: function () {
      return {
        tabActive: 0
      };
    },

    getInitialState: function () {
      return {
        tabActive: this.props.tabActive,

        // This array is used to store an information whether a tab
        // at specific index has already been created (e.g. selected
        // at least once).
        // If yes, it's rendered even if not currently selected.
        // This is because in some cases we don't want to re-create
        // tab content when it's being unselected/selected.
        // E.g. in case of an iframe being used as a tab-content
        // we want the iframe to stay in the DOM.
        created: [],
      };
    },

    componentDidMount: function () {
      let node = findDOMNode(this);
      node.addEventListener("keydown", this.onKeyDown, false);

      let index = this.state.tabActive;
      if (this.props.onMount) {
        this.props.onMount(index);
      }
    },

    componentWillReceiveProps: function (newProps) {
      if (newProps.tabActive) {
        let created = [...this.state.created];
        created[newProps.tabActive] = true;

        this.setState(Object.assign({}, this.state, {
          tabActive: newProps.tabActive,
          created: created,
        }));
      }
    },

    componentWillUnmount: function () {
      let node = findDOMNode(this);
      node.removeEventListener("keydown", this.onKeyDown, false);
    },

    // DOM Events

    onKeyDown: function (event) {
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
    },

    onClickTab: function (index, event) {
      this.setActive(index);
      event.preventDefault();
    },

    // API

    setActive: function (index) {
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
    },

    // Rendering

    renderMenuItems: function () {
      if (!this.props.children) {
        throw new Error("There must be at least one Tab");
      }

      if (!Array.isArray(this.props.children)) {
        this.props.children = [this.props.children];
      }

      let tabs = this.props.children
        .map(tab => {
          return typeof tab === "function" ? tab() : tab;
        }).filter(tab => {
          return tab;
        }).map((tab, index) => {
          let ref = ("tab-menu-" + index);
          let title = tab.props.title;
          let tabClassName = tab.props.className;
          let isTabSelected = this.state.tabActive === index;

          let classes = [
            "tabs-menu-item",
            tabClassName,
            isTabSelected ? "is-active" : ""
          ].join(" ");

          // Set tabindex to -1 (except the selected tab) so, it's focusable,
          // but not reachable via sequential tab-key navigation.
          // Changing selected tab (and so, moving focus) is done through
          // left and right arrow keys.
          // See also `onKeyDown()` event handler.
          return (
            DOM.li({
              ref: ref,
              key: index,
              id: "tab-" + index,
              className: classes,
              role: "presentation",
            },
              DOM.a({
                href: "#",
                tabIndex: this.state.tabActive === index ? 0 : -1,
                "aria-controls": "panel-" + index,
                "aria-selected": isTabSelected,
                role: "tab",
                onClick: this.onClickTab.bind(this, index),
              },
                title
              )
            )
          );
        });

      return (
        DOM.nav({className: "tabs-navigation"},
          DOM.ul({className: "tabs-menu", role: "tablist"},
            tabs
          )
        )
      );
    },

    renderPanels: function () {
      if (!this.props.children) {
        throw new Error("There must be at least one Tab");
      }

      if (!Array.isArray(this.props.children)) {
        this.props.children = [this.props.children];
      }

      let selectedIndex = this.state.tabActive;

      let panels = this.props.children
        .map(tab => {
          return typeof tab === "function" ? tab() : tab;
        }).filter(tab => {
          return tab;
        }).map((tab, index) => {
          let selected = selectedIndex == index;

          // Use 'visibility:hidden' + 'width/height:0' for hiding
          // content of non-selected tab. It's faster (not sure why)
          // than display:none and visibility:collapse.
          let style = {
            visibility: selected ? "visible" : "hidden",
            height: selected ? "100%" : "0",
            width: selected ? "100%" : "0",
          };

          return (
            DOM.div({
              key: index,
              id: "panel-" + index,
              style: style,
              className: "tab-panel-box",
              role: "tabpanel",
              "aria-labelledby": "tab-" + index,
            },
              (selected || this.state.created[index]) ? tab : null
            )
          );
        });

      return (
        DOM.div({className: "panels"},
          panels
        )
      );
    },

    render: function () {
      let classNames = ["tabs", this.props.className].join(" ");

      return (
        DOM.div({className: classNames},
          this.renderMenuItems(),
          this.renderPanels()
        )
      );
    },
  });

  /**
   * Renders simple tab 'panel'.
   */
  let Panel = React.createClass({
    displayName: "Panel",

    propTypes: {
      title: React.PropTypes.string.isRequired,
      children: React.PropTypes.oneOfType([
        React.PropTypes.array,
        React.PropTypes.element
      ]).isRequired
    },

    render: function () {
      return DOM.div({className: "tab-panel"},
        this.props.children
      );
    }
  });

  // Exports from this module
  exports.TabPanel = Panel;
  exports.Tabs = Tabs;
});
