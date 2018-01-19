/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimationDetailHeader = createFactory(require("./AnimationDetailHeader"));

class AnimationDetailContainer extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
    };
  }

  render() {
    const { animation } = this.props;

    return dom.div(
      {
        className: "animation-detail-container"
      },
      animation ?
      AnimationDetailHeader(
        {
          animation,
        }
      )
      :
      null
    );
  }
}

const mapStateToProps = state => {
  return {
    animation: state.animations.selectedAnimation,
  };
};

module.exports = connect(mapStateToProps)(AnimationDetailContainer);
