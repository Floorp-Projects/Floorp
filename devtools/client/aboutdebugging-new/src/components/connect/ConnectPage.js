/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const {
  USB_STATES,
} = require("../../constants");

const Actions = require("../../actions/index");

loader.lazyRequireGetter(this, "ADB_ADDON_STATES", "devtools/shared/adb/adb-addon", true);

const ConnectSection = createFactory(require("./ConnectSection"));
const ConnectSteps = createFactory(require("./ConnectSteps"));
const NetworkLocationsForm = createFactory(require("./NetworkLocationsForm"));
const NetworkLocationsList = createFactory(require("./NetworkLocationsList"));

const { PREFERENCES, PAGE_TYPES } = require("../../constants");

const USB_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const WIFI_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const GLOBE_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";

class ConnectPage extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      networkEnabled: PropTypes.bool.isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.string).isRequired,
      wifiEnabled: PropTypes.bool.isRequired,
    };
  }

  // TODO: avoid the use of this method
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1508688
  componentWillMount() {
    this.props.dispatch(Actions.selectPage(PAGE_TYPES.CONNECT));
  }

  renderWifi() {
    const { getString, wifiEnabled } = this.props;

    return Localized(
      {
        id: "about-debugging-connect-wifi",
        attrs: { title: true },
      },
      ConnectSection(
        {
          icon: WIFI_ICON_SRC,
          title: "Via WiFi",
        },
        wifiEnabled
          ? ConnectSteps(
            {
              steps: [
                getString("about-debugging-connect-wifi-step-same-network"),
                getString("about-debugging-connect-wifi-step-open-firefox"),
                getString("about-debugging-connect-wifi-step-open-options"),
                getString("about-debugging-connect-wifi-step-enable-debug"),
              ],
            })
          : Localized(
            {
              id: "about-debugging-connect-wifi-disabled",
              $pref: PREFERENCES.WIFI_ENABLED,
            },
            dom.div(
              {
                className: "connect-page__disabled-section",
              },
              "about-debugging-connect-wifi-disabled"
            )
          )
      )
    );
  }

  onToggleUSBClick() {
    const { adbAddonStatus } = this.props;
    const isAddonInstalled = adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    if (isAddonInstalled) {
      this.props.dispatch(Actions.uninstallAdbAddon());
    } else {
      this.props.dispatch(Actions.installAdbAddon());
    }
  }

  getUsbStatus() {
    switch (this.props.adbAddonStatus) {
      case ADB_ADDON_STATES.INSTALLED:
        return USB_STATES.ENABLED_USB;
      case ADB_ADDON_STATES.UNINSTALLED:
        return USB_STATES.DISABLED_USB;
      default:
        return USB_STATES.UPDATING_USB;
    }
  }

  renderUsbToggleButton() {
    const usbStatus = this.getUsbStatus();

    const localizedStates = {
      [USB_STATES.ENABLED_USB]: "about-debugging-connect-usb-disable-button",
      [USB_STATES.DISABLED_USB]: "about-debugging-connect-usb-enable-button",
      [USB_STATES.UPDATING_USB]: "about-debugging-connect-usb-updating-button",
    };
    const localizedState = localizedStates[usbStatus];

    // Disable the button while the USB status is updating.
    const disabled = usbStatus === USB_STATES.UPDATING_USB;

    return Localized(
      {
        id: localizedState,
      },
      dom.button(
        {
          className:
            "default-button connect-page__usb__toggle-button " +
            "js-connect-usb-toggle-button",
          disabled,
          onClick: () => this.onToggleUSBClick(),
        },
        localizedState
      )
    );
  }

  renderUsb() {
    const { adbAddonStatus, getString } = this.props;
    const isAddonInstalled = adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    return Localized(
      {
        id: "about-debugging-connect-usb",
        attrs: { title: true },
      },
      ConnectSection(
        {
          icon: USB_ICON_SRC,
          title: "Via USB",
        },
        isAddonInstalled
          ? ConnectSteps(
            {
              steps: [
                getString("about-debugging-connect-usb-step-enable-dev-menu"),
                getString("about-debugging-connect-usb-step-enable-debug"),
                getString("about-debugging-connect-usb-step-plug-device"),
              ],
            }
          )
          : Localized(
            {
              id: "about-debugging-connect-usb-disabled",
            },
            dom.aside(
              {
                className: "js-connect-usb-disabled-message",
              },
              "Enabling this will download and add the required Android USB debugging " +
                "components to Firefox."
            )
          ),
        this.renderUsbToggleButton()
      )
    );
  }

  renderNetwork() {
    const { dispatch, networkEnabled, networkLocations } = this.props;

    return Localized(
      {
        id: "about-debugging-connect-network",
        attrs: { title: true },
      },
      ConnectSection(
        {
          className: "connect-page__network",
          icon: GLOBE_ICON_SRC,
          title: "Via Network Location",
        },
        ...(networkEnabled
          ? [
              NetworkLocationsList({ dispatch, networkLocations }),
              dom.hr({ className: "separator separator--breathe" }),
              NetworkLocationsForm({ dispatch }),
          ]
          : [
              // We are using an array for this single element because of the spread
              // operator (...). The spread operator avoids React warnings about missing
              // keys.
              Localized(
                {
                  id: "about-debugging-connect-network-disabled",
                  $pref: PREFERENCES.NETWORK_ENABLED,
                },
                dom.div(
                  {
                    className: "connect-page__disabled-section",
                  },
                  "about-debugging-connect-network-disabled"
                )
              ),
          ])
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
          id: "about-debugging-connect-title",
        },
        dom.h1(
          {
            className: "alt-heading",
          },
          "Connect a Device"
        )
      ),
      this.renderWifi(),
      this.renderUsb(),
      this.renderNetwork()
    );
  }
}

module.exports = FluentReact.withLocalization(ConnectPage);
