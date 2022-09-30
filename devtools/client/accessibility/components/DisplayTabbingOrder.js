/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  label,
  input,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  updateDisplayTabbingOrder,
} = require("resource://devtools/client/accessibility/actions/ui.js");

class DisplayTabbingOrder extends PureComponent {
  static get propTypes() {
    return {
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
    const { tabbingOrderDisplayed } = this.props;
    return label(
      {
        className:
          "accessibility-tabbing-order devtools-checkbox-label devtools-ellipsis-text",
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
      }),
      L10N.getStr("accessibility.toolbar.displayTabbingOrder.label")
    );
  }
}

const mapStateToProps = ({ ui: { tabbingOrderDisplayed } }) => ({
  tabbingOrderDisplayed,
});

module.exports = connect(mapStateToProps)(DisplayTabbingOrder);
