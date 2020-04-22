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

class Footer extends PureComponent {
  render() {
    return dom.footer(
      {
        className: "compatibility-footer",
      },
      dom.button(
        {
          className: "compatibility-footer__feedback-button",
          title: "Feedback",
          onClick: () => openDocLink(FEEDBACK_LINK),
        },
        dom.img({
          className: "compatibility-footer__feedback-icon",
          src: "chrome://devtools/skin/images/report.svg",
        }),
        dom.label(
          {
            className: "compatibility-footer__feedback-label",
          },
          "Feedback"
        )
      )
    );
  }
}

module.exports = Footer;
