/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, PureComponent } =
  require("devtools/client/shared/vendor/react");

class AnimationTimelineTickList extends PureComponent {
  render() {
    return dom.div(
      {
        className: "animation-timeline-tick-list"
      }
    );
  }
}

module.exports = AnimationTimelineTickList;
