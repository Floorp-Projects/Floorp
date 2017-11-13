/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, DOM: dom, PureComponent } =
  require("devtools/client/shared/vendor/react");

const AnimationTimelineTickList = createFactory(require("./AnimationTimelineTickList"));

class AnimationListHeader extends PureComponent {
  render() {
    return dom.div(
      {
        className: "animation-list-header devtools-toolbar"
      },
      AnimationTimelineTickList()
    );
  }
}

module.exports = AnimationListHeader;
