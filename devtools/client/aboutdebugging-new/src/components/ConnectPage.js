/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  addNetworkLocation,
  removeNetworkLocation
} = require("../modules/network-locations");

const USB_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const WIFI_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const NETWORK_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-network-icon.svg";

class ConnectPage extends PureComponent {
  static get propTypes() {
    return {
      networkLocations: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      locationInputValue: ""
    };
  }

  renderSteps(steps) {
    return dom.ul(
      {
        className: "connect-page__steps"
      },
      steps.map(step => dom.li(
        {
          className: "connect-page__step"
        },
        step)
      )
    );
  }

  renderWifi() {
    return dom.section(
      {},
      dom.h2(
        {
          className: "connect-page__section__title"
        },
        dom.img(
          {
            className: "connect-page__section__icon",
            src: WIFI_ICON_SRC
          }
        ),
        "Via WiFi (Recommended)"
      ),
      this.renderSteps([
        "Ensure that your browser and device are on the same network",
        "Open Firefox for Android",
        "Go to Options -> Settings -> Advanced",
        "Enable Remote Debugging via WiFi in the Developer Tools section",
      ])
    );
  }

  renderUsb() {
    return dom.section(
      {},
      dom.h2(
        {
          className: "connect-page__section__title"
        },
        dom.img(
          {
            className: "connect-page__section__icon",
            src: USB_ICON_SRC
          }
        ),
        "Via USB"
      ),
      this.renderSteps([
        "Enable Developer menu on your Android device",
        "Enable USB Debugging on the Android Developer Menu",
        "Connect the USB Device to your computer",
      ])
    );
  }

  renderNetwork() {
    const { networkLocations } = this.props;
    return dom.section(
      {
        className: "connect-page__network"
      },
      dom.h2(
        {
          className: "connect-page__section__title"
        },
        dom.img(
          {
            className: "connect-page__section__icon",
            src: NETWORK_ICON_SRC
          }
        ),
        "Via Network Location"
      ),
      dom.ul(
        {},
        networkLocations.map(location =>
          dom.li(
            {
              className: "connect-page__network-location"
            },
            dom.span(
              {
                className: "ellipsis-text"
              },
              location
            ),
            dom.button(
              {
                className: "aboutdebugging-button",
                onClick: () => removeNetworkLocation(location)
              },
              "Remove"
            )
          )
        )
      ),
      dom.hr(
        {
          className: "connect-page__network__separator"
        }
      ),
      dom.form(
        {
          className: "connect-page__network-form",
          onSubmit: (e) => {
            addNetworkLocation(this.state.locationInputValue);
            this.setState({ locationInputValue: "" });
            e.preventDefault();
          }
        },
        dom.span({}, "Host:port"),
        dom.input({
          className: "connect-page__network-form__input",
          placeholder: "localhost:6080",
          type: "text",
          value: this.state.locationInputValue,
          onChange: (e) => {
            const locationInputValue = e.target.value;
            if (locationInputValue) {
              this.setState({ locationInputValue });
            }
          }
        }),
        dom.button({
          className: "aboutdebugging-button"
        }, "Add")
      )
    );
  }

  render() {
    return dom.article(
      {
        className: "page connect-page",
      },
      dom.h1(
        {
          className: "connect-page__title"
        },
        "Connect a Device"
      ),
      this.renderWifi(),
      this.renderUsb(),
      this.renderNetwork(),
    );
  }
}

module.exports = ConnectPage;
