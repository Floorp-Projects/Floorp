/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

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
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.string).isRequired,
    };
  }

  renderWifi() {
    const { getString } = this.props;
    return Localized(
      {
        id: "about-debugging-connect-wifi",
        attrs: { title: true }
      },
      ConnectSection(
        {
          icon: WIFI_ICON_SRC,
          title: "Via WiFi (Recommended)",
        },
        ConnectSteps({
          steps: [
            getString("about-debugging-connect-wifi-step-same-network"),
            getString("about-debugging-connect-wifi-step-open-firefox"),
            getString("about-debugging-connect-wifi-step-open-options"),
            getString("about-debugging-connect-wifi-step-enable-debug"),
          ]
        })
      )
    );
  }

  renderUsb() {
    const { getString } = this.props;
    return Localized(
      {
        id: "about-debugging-connect-usb",
        attrs: { title: true }
      },
      ConnectSection(
        {
          icon: USB_ICON_SRC,
          title: "Via USB",
        },
        ConnectSteps({
          steps: [
            getString("about-debugging-connect-usb-step-enable-dev-menu"),
            getString("about-debugging-connect-usb-step-enable-debug"),
            getString("about-debugging-connect-usb-step-plug-device"),
          ]
        })
      )
    );
  }

  renderNetwork() {
    const { dispatch, networkLocations } = this.props;
    return Localized(
      {
        id: "about-debugging-connect-network",
        attrs: { title: true }
      },
      ConnectSection(
        {
          className: "connect-page__network",
          icon: GLOBE_ICON_SRC,
          title: "Via Network Location",
        },
        NetworkLocationsList({ dispatch, networkLocations }),
        dom.hr({ className: "connect-page__network__separator" }),
        NetworkLocationsForm({ dispatch }),
      )
    );
  }

  render() {
    return dom.article(
      {
        className: "page connect-page js-connect-page",
      },
      Localized(
        {
          id: "about-debugging-connect-title"
        },
        dom.h1(
          {
            className: "connect-page__title"
          },
          "Connect a Device"
        )
      ),
      this.renderWifi(),
      this.renderUsb(),
      this.renderNetwork(),
    );
  }
}

module.exports = FluentReact.withLocalization(ConnectPage);
