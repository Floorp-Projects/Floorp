/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Message = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/Message.js")
);

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");
const {
  MESSAGE_LEVEL,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

class NetworkLocationsForm extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      networkLocations: PropTypes.arrayOf(Types.location).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      errorHostValue: null,
      errorMessageId: null,
      value: "",
    };
  }

  onSubmit(e) {
    const { networkLocations } = this.props;
    const { value } = this.state;

    e.preventDefault();

    if (!value) {
      return;
    }

    if (!value.match(/[^:]+:\d+/)) {
      this.setState({
        errorHostValue: value,
        errorMessageId: "about-debugging-network-location-form-invalid",
      });
      return;
    }

    if (networkLocations.includes(value)) {
      this.setState({
        errorHostValue: value,
        errorMessageId: "about-debugging-network-location-form-duplicate",
      });
      return;
    }

    this.props.dispatch(Actions.addNetworkLocation(value));
    this.setState({ errorHostValue: null, errorMessageId: null, value: "" });
  }

  renderError() {
    const { errorHostValue, errorMessageId } = this.state;

    if (!errorMessageId) {
      return null;
    }

    return Message(
      {
        className:
          "connect-page__network-form__error-message " +
          "qa-connect-page__network-form__error-message",
        level: MESSAGE_LEVEL.ERROR,
        isCloseable: true,
      },
      Localized(
        {
          id: errorMessageId,
          "$host-value": errorHostValue,
        },
        dom.p(
          {
            className: "technical-text",
          },
          errorMessageId
        )
      )
    );
  }

  render() {
    return dom.form(
      {
        className: "connect-page__network-form",
        onSubmit: e => this.onSubmit(e),
      },
      this.renderError(),
      Localized(
        {
          id: "about-debugging-network-locations-host-input-label",
        },
        dom.label(
          {
            htmlFor: "about-debugging-network-locations-host-input",
          },
          "Host"
        )
      ),
      dom.input({
        id: "about-debugging-network-locations-host-input",
        className: "default-input qa-network-form-input",
        placeholder: "localhost:6080",
        type: "text",
        value: this.state.value,
        onChange: e => {
          const value = e.target.value;
          this.setState({ value });
        },
      }),
      Localized(
        {
          id: "about-debugging-network-locations-add-button",
        },
        dom.button(
          {
            className: "primary-button qa-network-form-submit-button",
          },
          "Add"
        )
      )
    );
  }
}

module.exports = NetworkLocationsForm;
