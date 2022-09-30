/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

class KeyframeMarkerItem extends PureComponent {
  static get propTypes() {
    return {
      keyframe: PropTypes.object.isRequired,
    };
  }

  render() {
    const { keyframe } = this.props;

    return dom.li({
      className: "keyframe-marker-item",
      title: keyframe.value,
      style: {
        marginInlineStart: `${keyframe.offset * 100}%`,
      },
    });
  }
}

module.exports = KeyframeMarkerItem;
