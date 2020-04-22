/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);

const FEEDBACK_LINK =
  "https://docs.google.com/forms/d/e/1FAIpQLSeevOHveQ1tDuKYY5Fxyb3vvbKKumdLWUT5-RuwJWoAAOST5g/viewform";

const REPORT_ICON = "chrome://devtools/skin/images/report.svg";
const SETTINGS_ICON = "chrome://devtools/skin/images/settings.svg";

class Footer extends PureComponent {
  _renderButton(icon, label, onClick) {
    return dom.button(
      {
        className: "compatibility-footer__button",
        title: label,
        onClick,
      },
      dom.img({
        className: "compatibility-footer__icon",
        src: icon,
      }),
      dom.label(
        {
          className: "compatibility-footer__label",
        },
        label
      )
    );
  }

  render() {
    return dom.footer(
      {
        className: "compatibility-footer",
      },
      this._renderButton(SETTINGS_ICON, "Settings"),
      this._renderButton(REPORT_ICON, "Feedback", () =>
        openDocLink(FEEDBACK_LINK)
      )
    );
  }
}

module.exports = Footer;
