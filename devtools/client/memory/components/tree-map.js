/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass } = require("devtools/client/shared/vendor/react");
const { treeMapModel } = require("../models");
const startVisualization = require("./tree-map/start");

module.exports = createClass({
  displayName: "TreeMap",

  propTypes: {
    treeMap: treeMapModel
  },

  getInitialState() {
    return {};
  },

  componentDidMount() {
    const { treeMap } = this.props;
    if (treeMap && treeMap.report) {
      this._startVisualization();
    }
  },

  shouldComponentUpdate(nextProps) {
    const oldTreeMap = this.props.treeMap;
    const newTreeMap = nextProps.treeMap;
    return oldTreeMap !== newTreeMap;
  },

  componentDidUpdate(prevProps) {
    this._stopVisualization();

    if (this.props.treeMap && this.props.treeMap.report) {
      this._startVisualization();
    }
  },

  componentWillUnmount() {
    if (this.state.stopVisualization) {
      this.state.stopVisualization();
    }
  },

  _stopVisualization() {
    if (this.state.stopVisualization) {
      this.state.stopVisualization();
      this.setState({ stopVisualization: null });
    }
  },

  _startVisualization() {
    const { container } = this.refs;
    const { report } = this.props.treeMap;
    const stopVisualization = startVisualization(container, report);
    this.setState({ stopVisualization });
  },

  render() {
    return dom.div(
      {
        ref: "container",
        className: "tree-map-container"
      }
    );
  }
});
