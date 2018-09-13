/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ConnectSection = createFactory(require("./ConnectSection"));
const ConnectSteps = createFactory(require("./ConnectSteps"));
const NetworkLocationsForm = createFactory(require("./NetworkLocationsForm"));
const NetworkLocationsList = createFactory(require("./NetworkLocationsList"));

const USB_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const WIFI_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const GLOBE_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";

class ConnectPage extends PureComponent {
  static get propTypes() {
    return {
      networkLocations: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  renderWifi() {
    return ConnectSection(
      {
        icon: WIFI_ICON_SRC,
        title: "Via WiFi (Recommended)",
      },
      ConnectSteps({
        steps: [
          "Ensure that your browser and device are on the same network",
          "Open Firefox for Android",
          "Go to Options -> Settings -> Advanced",
          "Enable Remote Debugging via WiFi in the Developer Tools section",
        ]
      })
    );
  }

  renderUsb() {
    return ConnectSection(
      {
        icon: USB_ICON_SRC,
        title: "Via USB",
      },
      ConnectSteps({
        steps: [
          "Enable Developer menu on your Android device",
          "Enable USB Debugging on the Android Developer Menu",
          "Connect the USB Device to your computer",
        ]
      })
    );
  }

  renderNetwork() {
    const { networkLocations } = this.props;
    return ConnectSection(
      {
        className: "connect-page__network",
        icon: GLOBE_ICON_SRC,
        title: "Via Network Location",
      },
      NetworkLocationsList({ networkLocations }),
      dom.hr({ className: "connect-page__network__separator" }),
      NetworkLocationsForm(),
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
