/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("devtools/client/inspector/fonts/types");

class FontName extends PureComponent {
  static get propTypes() {
    return {
      font: PropTypes.shape(Types.font).isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onNameMouseOver = this.onNameMouseOver.bind(this);
    this.onNameMouseOut = this.onNameMouseOut.bind(this);
  }

  onNameMouseOver() {
    const { font, onToggleFontHighlight } = this.props;

    onToggleFontHighlight(font, true);
  }

  onNameMouseOut() {
    const { font, onToggleFontHighlight } = this.props;

    onToggleFontHighlight(font, false);
  }

  render() {
    return dom.span(
      {
        className: "font-name",
        onMouseOver: this.onNameMouseOver,
        onMouseOut: this.onNameMouseOut,
      },
      this.props.font.name
    );
  }
}

module.exports = FontName;
