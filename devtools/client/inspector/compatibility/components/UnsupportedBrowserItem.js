/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

const ICONS = {
  firefox: {
    src: "chrome://devtools/skin/images/browsers/firefox.svg",
    isMobileIconNeeded: false,
  },
  firefox_android: {
    src: "chrome://devtools/skin/images/browsers/firefox.svg",
    isMobileIconNeeded: true,
  },
  chrome: {
    src: "chrome://devtools/skin/images/browsers/chrome.svg",
    isMobileIconNeeded: false,
  },
  chrome_android: {
    src: "chrome://devtools/skin/images/browsers/chrome.svg",
    isMobileIconNeeded: true,
  },
  safari: {
    src: "chrome://devtools/skin/images/browsers/safari.svg",
    isMobileIconNeeded: false,
  },
  safari_ios: {
    src: "chrome://devtools/skin/images/browsers/safari.svg",
    isMobileIconNeeded: true,
  },
  edge: {
    src: "chrome://devtools/skin/images/browsers/edge.svg",
    isMobileIconNeeded: false,
  },
};

class UnsupportedBrowserItem extends PureComponent {
  static get propTypes() {
    return {
      id: Types.browser.id,
      name: Types.browser.name,
      versions: PropTypes.arrayOf(
        PropTypes.shape({
          alias: Types.browser.status,
          version: Types.browser.version,
        })
      ),
    };
  }

  render() {
    const { id, name, versions } = this.props;

    const versionsTitle = versions
      .map(({ alias, version }) => version + (alias ? `(${alias})` : ""))
      .join(", ");
    const title = `${name} ${versionsTitle}`;

    const icon = ICONS[id];

    return dom.li(
      {
        className:
          "compatibility-unsupported-browser-item" +
          (icon.isMobileIconNeeded
            ? " compatibility-unsupported-browser-item--mobile"
            : ""),
      },
      dom.img({
        className: "compatibility-unsupported-browser-item__icon",
        alt: title,
        title,
        src: icon.src,
      })
    );
  }
}

module.exports = UnsupportedBrowserItem;
