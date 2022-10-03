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

const {
  USB_STATES,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");

loader.lazyRequireGetter(
  this,
  "ADB_ADDON_STATES",
  "resource://devtools/client/shared/remote-debugging/adb/adb-addon.js",
  true
);

const Link = createFactory(
  require("resource://devtools/client/shared/vendor/react-router-dom.js").Link
);
const ConnectSection = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/connect/ConnectSection.js")
);
const ConnectSteps = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/connect/ConnectSteps.js")
);
const NetworkLocationsForm = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/connect/NetworkLocationsForm.js")
);
const NetworkLocationsList = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/connect/NetworkLocationsList.js")
);

const {
  PAGE_TYPES,
  RUNTIMES,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

const USB_ICON_SRC =
  "chrome://devtools/skin/images/aboutdebugging-usb-icon.svg";
const GLOBE_ICON_SRC =
  "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";

const TROUBLESHOOT_USB_URL =
  "https://firefox-source-docs.mozilla.org/devtools-user/about_colon_debugging/index.html#connecting-to-a-remote-device";
const TROUBLESHOOT_NETWORK_URL =
  "https://firefox-source-docs.mozilla.org/devtools-user/about_colon_debugging/index.html#connecting-over-the-network";

class ConnectPage extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: Types.adbAddonStatus,
      dispatch: PropTypes.func.isRequired,
      networkLocations: PropTypes.arrayOf(Types.location).isRequired,
    };
  }

  // TODO: avoid the use of this method
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    this.props.dispatch(Actions.selectPage(PAGE_TYPES.CONNECT));
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

  renderUsbStatus() {
    const statusTextId = {
      [USB_STATES.ENABLED_USB]: "about-debugging-setup-usb-status-enabled",
      [USB_STATES.DISABLED_USB]: "about-debugging-setup-usb-status-disabled",
      [USB_STATES.UPDATING_USB]: "about-debugging-setup-usb-status-updating",
    }[this.getUsbStatus()];

    return Localized(
      {
        id: statusTextId,
      },
      dom.span(
        {
          className: "connect-page__usb-section__heading__status",
        },
        statusTextId
      )
    );
  }

  renderUsbToggleButton() {
    const usbStatus = this.getUsbStatus();

    const localizedStates = {
      [USB_STATES.ENABLED_USB]: "about-debugging-setup-usb-disable-button",
      [USB_STATES.DISABLED_USB]: "about-debugging-setup-usb-enable-button",
      [USB_STATES.UPDATING_USB]: "about-debugging-setup-usb-updating-button",
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
            "default-button connect-page__usb-section__heading__toggle " +
            "qa-connect-usb-toggle-button",
          disabled,
          onClick: () => this.onToggleUSBClick(),
        },
        localizedState
      )
    );
  }

  renderUsb() {
    const { adbAddonStatus } = this.props;
    const isAddonInstalled = adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    return ConnectSection(
      {
        icon: USB_ICON_SRC,
        title: dom.div(
          {
            className: "connect-page__usb-section__heading",
          },
          Localized(
            { id: "about-debugging-setup-usb-title" },
            dom.span(
              {
                className: "connect-page__usb-section__heading__title",
              },
              "USB"
            )
          ),
          this.renderUsbStatus(),
          this.renderUsbToggleButton()
        ),
      },
      isAddonInstalled
        ? ConnectSteps({
            steps: [
              {
                localizationId:
                  "about-debugging-setup-usb-step-enable-dev-menu2",
              },
              {
                localizationId: "about-debugging-setup-usb-step-enable-debug2",
              },
              {
                localizationId:
                  "about-debugging-setup-usb-step-enable-debug-firefox2",
              },
              {
                localizationId: "about-debugging-setup-usb-step-plug-device",
              },
            ],
          })
        : Localized(
            {
              id: "about-debugging-setup-usb-disabled",
            },
            dom.aside(
              {
                className: "qa-connect-usb-disabled-message",
              },
              "Enabling this will download and add the required Android USB debugging " +
                "components to Firefox."
            )
          ),
      this.renderTroubleshootText(RUNTIMES.USB)
    );
  }

  renderNetwork() {
    const { dispatch, networkLocations } = this.props;

    return Localized(
      {
        id: "about-debugging-setup-network",
        attrs: { title: true },
      },
      ConnectSection({
        icon: GLOBE_ICON_SRC,
        title: "Network Location",
        extraContent: dom.div(
          {},
          NetworkLocationsList({ dispatch, networkLocations }),
          NetworkLocationsForm({ dispatch, networkLocations }),
          this.renderTroubleshootText(RUNTIMES.NETWORK)
        ),
      })
    );
  }

  renderTroubleshootText(connectionType) {
    const localizationId =
      connectionType === RUNTIMES.USB
        ? "about-debugging-setup-usb-troubleshoot"
        : "about-debugging-setup-network-troubleshoot";

    const className =
      "connect-page__troubleshoot connect-page__troubleshoot--" +
      `${connectionType === RUNTIMES.USB ? "usb" : "network"}`;

    const url =
      connectionType === RUNTIMES.USB
        ? TROUBLESHOOT_USB_URL
        : TROUBLESHOOT_NETWORK_URL;

    return dom.aside(
      {
        className,
      },
      Localized(
        {
          id: localizationId,
          a: dom.a({
            href: url,
            target: "_blank",
          }),
        },
        dom.p({}, localizationId)
      )
    );
  }

  render() {
    return dom.article(
      {
        className: "page connect-page qa-connect-page",
      },
      Localized(
        {
          id: "about-debugging-setup-title",
        },
        dom.h1(
          {
            className: "alt-heading alt-heading--larger",
          },
          "Setup"
        )
      ),
      Localized(
        {
          id: "about-debugging-setup-intro",
        },
        dom.p(
          {},
          "Configure the connection method you wish to remotely debug your device with."
        )
      ),
      Localized(
        {
          id: "about-debugging-setup-this-firefox2",
          a: Link({
            to: `/runtime/${RUNTIMES.THIS_FIREFOX}`,
          }),
        },
        dom.p({}, "about-debugging-setup-this-firefox")
      ),
      dom.section(
        {
          className: "connect-page__breather",
        },
        Localized(
          {
            id: "about-debugging-setup-connect-heading",
          },
          dom.h2(
            {
              className: "alt-heading",
            },
            "Connect a device"
          )
        ),
        this.renderUsb(),
        this.renderNetwork()
      )
    );
  }
}

module.exports = FluentReact.withLocalization(ConnectPage);
