/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimatedPropertyItem = createFactory(require("./AnimatedPropertyItem"));

class AnimatedPropertyList extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      animatedPropertyMap: null
    };
  }

  componentDidMount() {
    this.updateKeyframesList(this.props.animation);
  }

  componentWillReceiveProps(nextProps) {
    this.updateKeyframesList(nextProps.animation);
  }

  async updateKeyframesList(animation) {
    const {
      getAnimatedPropertyMap,
      emitEventForTest,
    } = this.props;
    const animatedPropertyMap = await getAnimatedPropertyMap(animation);

    this.setState({ animatedPropertyMap });

    emitEventForTest("animation-keyframes-rendered");
  }

  render() {
    const { animatedPropertyMap } = this.state;

    if (!animatedPropertyMap) {
      return null;
    }

    return dom.ul(
      {
        className: "animated-property-list"
      },
      [...animatedPropertyMap.entries()].map(([property, values]) => {
        return AnimatedPropertyItem(
          {
            property,
            values,
          }
        );
      })
    );
  }
}

module.exports = AnimatedPropertyList;
