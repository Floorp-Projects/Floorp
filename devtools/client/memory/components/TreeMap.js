/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const { treeMapModel } = require("resource://devtools/client/memory/models.js");
const startVisualization = require("resource://devtools/client/memory/components/tree-map/start.js");

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
    if (treeMap?.report) {
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
