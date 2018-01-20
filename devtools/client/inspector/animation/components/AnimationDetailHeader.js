/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getFormattedTitle } = require("../utils/l10n");

class AnimationDetailHeader extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      setDetailVisibility: PropTypes.func.isRequired,
    };
  }

  onClick() {
    const { setDetailVisibility } = this.props;
    setDetailVisibility(false);
  }

  render() {
    const { animation } = this.props;

    return dom.div(
      {
        className: "animation-detail-header devtools-toolbar",
      },
      dom.div(
        {
          className: "animation-detail-title",
        },
        getFormattedTitle(animation.state)
      ),
      dom.button(
        {
          className: "animation-detail-close-button devtools-button",
          onClick: this.onClick.bind(this),
        }
      )
    );
  }
}

module.exports = AnimationDetailHeader;
