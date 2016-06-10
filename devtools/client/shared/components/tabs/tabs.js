/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const React = require("devtools/client/shared/vendor/react");
  const DOM = React.DOM;

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
   *  <article class='tab-panel'>
   *    The content of active panel here
   *  </article>
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
        tabActive: 1
      };
    },

    getInitialState: function () {
      return {
        tabActive: this.props.tabActive
      };
    },

    componentDidMount: function () {
      let index = this.state.tabActive;
      if (this.props.onMount) {
        this.props.onMount(index);
      }
    },

    componentWillReceiveProps: function (newProps) {
      if (newProps.tabActive) {
        this.setState({tabActive: newProps.tabActive});
      }
    },

    setActive: function (index, e) {
      let onAfterChange = this.props.onAfterChange;
      let onBeforeChange = this.props.onBeforeChange;

      if (onBeforeChange) {
        let cancel = onBeforeChange(index);
        if (cancel) {
          return;
        }
      }

      let newState = {
        tabActive: index
      };

      this.setState(newState, () => {
        if (onAfterChange) {
          onAfterChange(index);
        }
      });

      e.preventDefault();
    },

    getMenuItems: function () {
      if (!this.props.children) {
        throw new Error("Tabs must contain at least one Panel");
      }

      if (!Array.isArray(this.props.children)) {
        this.props.children = [this.props.children];
      }

      let menuItems = this.props.children
        .map(function (panel) {
          return typeof panel === "function" ? panel() : panel;
        }).filter(function (panel) {
          return panel;
        }).map(function (panel, index) {
          let ref = ("tab-menu-" + (index + 1));
          let title = panel.props.title;
          let tabClassName = panel.props.className;

          let classes = [
            "tabs-menu-item",
            tabClassName,
            this.state.tabActive === (index + 1) && "is-active"
          ].join(" ");

          return (
            DOM.li({ref: ref, key: index, className: classes},
              DOM.a({href: "#", onClick: this.setActive.bind(this, index + 1)},
                title
              )
            )
          );
        }.bind(this));

      return (
        DOM.nav({className: "tabs-navigation"},
          DOM.ul({className: "tabs-menu"},
            menuItems
          )
        )
      );
    },

    getSelectedPanel: function () {
      let index = this.state.tabActive - 1;
      let panel = this.props.children[index];

      return (
        DOM.article({ref: "tab-panel", className: "tab-panel"},
          panel
        )
      );
    },

    render: function () {
      let classNames = ["tabs", this.props.className].join(" ");

      return (
        DOM.div({className: classNames},
          this.getMenuItems(),
          this.getSelectedPanel()
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
      return DOM.div({},
        this.props.children
      );
    }
  });

  // Exports from this module
  exports.TabPanel = Panel;
  exports.Tabs = Tabs;
});
