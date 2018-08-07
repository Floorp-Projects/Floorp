/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const USB_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const WIFI_ICON_SRC = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";

class RuntimePage extends PureComponent {
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
    );
  }
}

module.exports = RuntimePage;
