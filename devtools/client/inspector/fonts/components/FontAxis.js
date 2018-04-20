/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class FontAxis extends PureComponent {
  static get propTypes() {
    return {
      defaultValue: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      label: PropTypes.string.isRequired,
      min: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      max: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      name: PropTypes.string.isRequired,
      onChange: PropTypes.func.isRequired,
      showInput: PropTypes.bool,
      step: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      value: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.onChange = this.onChange.bind(this);
  }

  onChange(e) {
    this.props.onChange(this.props.name, e.target.value);
  }

  render() {
    const defaults = {
      min: this.props.min,
      max: this.props.max,
      onChange: this.onChange,
      step: this.props.step || 1,
      value: this.props.value || this.props.defaultValue,
    };

    const range = dom.input(
      {
        ...defaults,
        className: "font-axis-slider",
        title: this.props.label,
        type: "range",
      }
    );

    const input = dom.input(
      {
        ...defaults,
        className: "font-axis-input",
        type: "number",
      }
    );

    return dom.label(
      {
        className: "font-control",
      },
      dom.span(
        {
          className: "font-control-label",
        },
        this.props.label
      ),
      dom.div(
        {
          className: "font-control-input"
        },
        range,
        this.props.showInput ? input : null
      )
    );
  }
}

module.exports = FontAxis;
