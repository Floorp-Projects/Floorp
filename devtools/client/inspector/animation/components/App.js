/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, DOM: dom, PropTypes, PureComponent } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const AnimationListContainer = createFactory(require("./AnimationListContainer"));
const NoAnimationPanel = createFactory(require("./NoAnimationPanel"));

class App extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      toggleElementPicker: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.animations.length !== 0 || nextProps.animations.length !== 0;
  }

  render() {
    const { animations, toggleElementPicker } = this.props;

    return dom.div(
      {
        id: "animation-container"
      },
      animations.length ?
      AnimationListContainer(
        {
          animations
        }
      )
      :
      NoAnimationPanel(
        {
          toggleElementPicker
        }
      )
    );
  }
}

module.exports = connect(state => state)(App);
