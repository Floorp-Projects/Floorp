/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimatedPropertyName = createFactory(require("./AnimatedPropertyName"));
const KeyframesGraph = createFactory(require("./keyframes-graph/KeyframesGraph"));

class AnimatedPropertyItem extends PureComponent {
  static get propTypes() {
    return {
      property: PropTypes.string.isRequired,
      state: PropTypes.object.isRequired,
      values: PropTypes.array.isRequired,
    };
  }

  render() {
    const {
      property,
      state,
    } = this.props;

    return dom.li(
      {
        className: "animated-property-item"
      },
      AnimatedPropertyName(
        {
          property,
          state,
        }
      ),
      KeyframesGraph()
    );
  }
}

module.exports = AnimatedPropertyItem;
