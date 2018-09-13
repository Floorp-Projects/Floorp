/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const {
  addNetworkLocation
} = require("../../modules/network-locations");

class NetworkLocationsForm extends PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      value: ""
    };
  }

  render() {
    return dom.form(
      {
        className: "connect-page__network-form",
        onSubmit: (e) => {
          const { value } = this.state;
          if (value) {
            addNetworkLocation(value);
            this.setState({ value: "" });
          }
          e.preventDefault();
        }
      },
      dom.span({}, "Host:port"),
      dom.input({
        className: "connect-page__network-form__input",
        placeholder: "localhost:6080",
        type: "text",
        value: this.state.value,
        onChange: (e) => {
          const value = e.target.value;
          this.setState({ value });
        }
      }),
      dom.button({
        className: "aboutdebugging-button"
      }, "Add")
    );
  }
}

module.exports = NetworkLocationsForm;
