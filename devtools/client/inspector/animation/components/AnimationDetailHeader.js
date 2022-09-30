/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  getFormattedTitle,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");

class AnimationDetailHeader extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      setDetailVisibility: PropTypes.func.isRequired,
    };
  }

  onClick(event) {
    event.stopPropagation();
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
      dom.button({
        className: "animation-detail-close-button devtools-button",
        onClick: this.onClick.bind(this),
      })
    );
  }
}

module.exports = AnimationDetailHeader;
