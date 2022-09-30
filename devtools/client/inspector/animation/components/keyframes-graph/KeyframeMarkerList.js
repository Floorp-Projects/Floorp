/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const KeyframeMarkerItem = createFactory(
  require("resource://devtools/client/inspector/animation/components/keyframes-graph/KeyframeMarkerItem.js")
);

class KeyframeMarkerList extends PureComponent {
  static get propTypes() {
    return {
      keyframes: PropTypes.array.isRequired,
    };
  }

  render() {
    const { keyframes } = this.props;

    return dom.ul(
      {
        className: "keyframe-marker-list",
      },
      keyframes.map(keyframe => KeyframeMarkerItem({ keyframe }))
    );
  }
}

module.exports = KeyframeMarkerList;
