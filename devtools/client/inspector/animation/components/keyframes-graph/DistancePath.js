/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");

const ComputedStylePath = require("devtools/client/inspector/animation/components/keyframes-graph/ComputedStylePath");

class DistancePath extends ComputedStylePath {
  getPropertyName() {
    return "opacity";
  }

  getPropertyValue(keyframe) {
    return keyframe.distance;
  }

  toSegmentValue(computedStyle) {
    return computedStyle;
  }

  render() {
    return dom.g(
      {
        className: "distance-path",
      },
      super.renderGraph()
    );
  }
}

module.exports = DistancePath;
