/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../../actions/index");

class NetworkLocationsForm extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      value: "",
    };
  }

  render() {
    return dom.form(
      {
        className: "connect-page__network-form",
        onSubmit: (e) => {
          const { value } = this.state;
          if (value) {
            this.props.dispatch(Actions.addNetworkLocation(value));
            this.setState({ value: "" });
          }
          e.preventDefault();
        },
      },
      Localized(
        {
          id: "about-debugging-network-locations-host-input-label",
        },
        dom.label(
          {
            htmlFor: "about-debugging-network-locations-host-input",
          },
          "Host",
        )
      ),
      dom.input({
        id: "about-debugging-network-locations-host-input",
        className: "std-input js-network-form-input",
        placeholder: "localhost:6080",
        type: "text",
        value: this.state.value,
        onChange: (e) => {
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
            className: "std-button js-network-form-submit-button",
          },
          "Add"
        )
      )
    );
  }
}

module.exports = NetworkLocationsForm;
