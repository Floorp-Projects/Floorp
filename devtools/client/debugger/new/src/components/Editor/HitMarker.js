"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const markerEl = document.createElement("div");

function makeMarker() {
  const marker = markerEl.cloneNode(true);
  marker.className = "editor hit-marker";
  return marker;
}

class HitMarker extends _react.Component {
  addMarker() {
    const hitData = this.props.hitData;
    const line = hitData.line - 1;
    this.props.editor.setGutterMarker(line, "hit-markers", makeMarker());
    this.props.editor.addLineClass(line, "line", "hit-marker");
  }

  shouldComponentUpdate(nextProps) {
    return this.props.editor !== nextProps.editor || this.props.hitData !== nextProps.hitData;
  }

  componentDidMount() {
    this.addMarker();
  }

  componentDidUpdate() {
    this.addMarker();
  }

  componentWillUnmount() {
    const hitData = this.props.hitData;
    const line = hitData.line - 1;
    this.props.editor.setGutterMarker(line, "hit-markers", null);
    this.props.editor.removeLineClass(line, "line", "hit-marker");
  }

  render() {
    return null;
  }

}

exports.default = HitMarker;