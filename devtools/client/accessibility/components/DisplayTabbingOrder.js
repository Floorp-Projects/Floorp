/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  label,
  input,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { L10N } = require("devtools/client/accessibility/utils/l10n");

const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  updateDisplayTabbingOrder,
} = require("devtools/client/accessibility/actions/ui");

class DisplayTabbingOrder extends PureComponent {
  static get propTypes() {
    return {
      describedby: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      tabbingOrderDisplayed: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      disabled: false,
    };

    this.onChange = this.onChange.bind(this);
  }

  async onChange() {
    const { dispatch, tabbingOrderDisplayed } = this.props;

    this.setState({ disabled: true });
    await dispatch(updateDisplayTabbingOrder(!tabbingOrderDisplayed));
    this.setState({ disabled: false });
  }

  render() {
    const { describedby, tabbingOrderDisplayed } = this.props;
    return label(
      {
        className: "accessibility-tabbing-order devtools-checkbox-label",
        htmlFor: "devtools-display-tabbing-order-checkbox",
        title: L10N.getStr("accessibility.toolbar.displayTabbingOrder.tooltip"),
      },
      input({
        id: "devtools-display-tabbing-order-checkbox",
        className: "devtools-checkbox",
        type: "checkbox",
        checked: tabbingOrderDisplayed,
        disabled: this.state.disabled,
        onChange: this.onChange,
        "aria-describedby": describedby,
      }),
      L10N.getStr("accessibility.toolbar.displayTabbingOrder.label")
    );
  }
}

const mapStateToProps = ({ ui: { tabbingOrderDisplayed } }) => ({
  tabbingOrderDisplayed,
});

module.exports = connect(mapStateToProps)(DisplayTabbingOrder);
