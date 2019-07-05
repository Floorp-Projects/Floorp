/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { treeMapModel } = require("../models");
const startVisualization = require("./tree-map/start");

class TreeMap extends Component {
  static get propTypes() {
    return {
      treeMap: treeMapModel,
    };
  }

  constructor(props) {
    super(props);
    this.state = {};
    this._stopVisualization = this._stopVisualization.bind(this);
    this._startVisualization = this._startVisualization.bind(this);
  }

  componentDidMount() {
    const { treeMap } = this.props;
    if (treeMap && treeMap.report) {
      this._startVisualization();
    }
  }

  shouldComponentUpdate(nextProps) {
    const oldTreeMap = this.props.treeMap;
    const newTreeMap = nextProps.treeMap;
    return oldTreeMap !== newTreeMap;
  }

  componentDidUpdate(prevProps) {
    this._stopVisualization();

    if (this.props.treeMap && this.props.treeMap.report) {
      this._startVisualization();
    }
  }

  componentWillUnmount() {
    if (this.state.stopVisualization) {
      this.state.stopVisualization();
    }
  }

  _stopVisualization() {
    if (this.state.stopVisualization) {
      this.state.stopVisualization();
      this.setState({ stopVisualization: null });
    }
  }

  _startVisualization() {
    const { container } = this.refs;
    const { report } = this.props.treeMap;
    const stopVisualization = startVisualization(container, report);
    this.setState({ stopVisualization });
  }

  render() {
    return dom.div({
      ref: "container",
      className: "tree-map-container",
    });
  }
}

module.exports = TreeMap;
