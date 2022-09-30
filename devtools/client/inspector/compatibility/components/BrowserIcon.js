/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const Types = require("resource://devtools/client/inspector/compatibility/types.js");

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
  ie: {
    src: "chrome://devtools/skin/images/browsers/ie.svg",
    isMobileIconNeeded: false,
  },
};

class BrowserIcon extends PureComponent {
  static get propTypes() {
    return {
      id: Types.browser.id,
      title: PropTypes.string,
      name: PropTypes.string,
    };
  }

  render() {
    const { id, name, title } = this.props;

    const icon = ICONS[id];

    return dom.span(
      {
        className:
          "compatibility-browser-icon" +
          (icon.isMobileIconNeeded
            ? " compatibility-browser-icon--mobile"
            : ""),
      },
      dom.img({
        className: "compatibility-browser-icon__image",
        alt: name || title,
        title,
        src: icon.src,
      })
    );
  }
}

module.exports = BrowserIcon;
