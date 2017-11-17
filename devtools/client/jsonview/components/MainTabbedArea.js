/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { createFactories } = require("devtools/client/shared/react-utils");
  const { JsonPanel } = createFactories(require("./JsonPanel"));
  const { TextPanel } = createFactories(require("./TextPanel"));
  const { HeadersPanel } = createFactories(require("./HeadersPanel"));
  const { Tabs, TabPanel } = createFactories(require("devtools/client/shared/components/tabs/Tabs"));

  /**
   * This object represents the root application template
   * responsible for rendering the basic tab layout.
   */
  class MainTabbedArea extends Component {
    static get propTypes() {
      return {
        jsonText: PropTypes.string,
        tabActive: PropTypes.number,
        actions: PropTypes.object,
        headers: PropTypes.object,
        searchFilter: PropTypes.string,
        json: PropTypes.oneOfType([
          PropTypes.string,
          PropTypes.object,
          PropTypes.array,
          PropTypes.bool,
          PropTypes.number
        ]),
        expandedNodes: PropTypes.instanceOf(Set),
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        json: {},
        headers: {},
        jsonText: props.jsonText,
        tabActive: props.tabActive
      };

      this.onTabChanged = this.onTabChanged.bind(this);
    }

    onTabChanged(index) {
      this.setState({tabActive: index});
    }

    render() {
      return (
        Tabs({
          tabActive: this.state.tabActive,
          onAfterChange: this.onTabChanged},
          TabPanel({
            className: "json",
            title: JSONView.Locale.$STR("jsonViewer.tab.JSON")},
            JsonPanel({
              data: this.props.json,
              expandedNodes: this.props.expandedNodes,
              actions: this.props.actions,
              searchFilter: this.state.searchFilter
            })
          ),
          TabPanel({
            className: "rawdata",
            title: JSONView.Locale.$STR("jsonViewer.tab.RawData")},
            TextPanel({
              isValidJson: !(this.props.json instanceof Error),
              data: this.state.jsonText,
              actions: this.props.actions
            })
          ),
          TabPanel({
            className: "headers",
            title: JSONView.Locale.$STR("jsonViewer.tab.Headers")},
            HeadersPanel({
              data: this.props.headers,
              actions: this.props.actions,
              searchFilter: this.props.searchFilter
            })
          )
        )
      );
    }
  }

  // Exports from this module
  exports.MainTabbedArea = MainTabbedArea;
});
