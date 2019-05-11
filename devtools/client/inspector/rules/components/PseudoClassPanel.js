/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Types = require("../types");

class PseudoClassPanel extends PureComponent {
  static get propTypes() {
    return {
      onTogglePseudoClass: PropTypes.func.isRequired,
      pseudoClasses: PropTypes.shape(Types.pseudoClasses).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onInputChange = this.onInputChange.bind(this);
  }

  onInputChange(event) {
    this.props.onTogglePseudoClass(event.target.value);
  }

  render() {
    const { pseudoClasses } = this.props;

    return (
      dom.div(
        {
          id: "pseudo-class-panel",
          className: "theme-toolbar ruleview-reveal-panel",
        },
        Object.entries(pseudoClasses).map(([value, { isChecked, isDisabled }]) => {
          return (
            dom.label({},
              dom.input({
                key: value,
                id: `pseudo-${value.slice(1)}-toggle`,
                checked: isChecked,
                disabled: isDisabled,
                onChange: this.onInputChange,
                tabIndex: 0,
                type: "checkbox",
                value,
              }),
              value
            )
          );
        })
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    pseudoClasses: state.pseudoClasses,
  };
};

module.exports = connect(mapStateToProps)(PseudoClassPanel);
