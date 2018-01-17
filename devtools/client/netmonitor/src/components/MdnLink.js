/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { gDevTools } = require("devtools/client/framework/devtools");
const { L10N } = require("../utils/l10n");

const { a } = dom;

const LEARN_MORE = L10N.getStr("netmonitor.headers.learnMore");

function MDNLink({ url }) {
  return (
    a({
      className: "devtools-button learn-more-link",
      title: LEARN_MORE,
      onClick: (e) => onLearnMoreClick(e, url),
    })
  );
}

MDNLink.displayName = "MDNLink";

MDNLink.propTypes = {
  url: PropTypes.string.isRequired,
};

function onLearnMoreClick(e, url) {
  e.stopPropagation();
  e.preventDefault();

  let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  let { button, ctrlKey, metaKey } = e;
  let isOSX = Services.appinfo.OS == "Darwin";
  if (button === 1 || (button === 0 && (isOSX ? metaKey : ctrlKey))) {
    win.openUILinkIn(url, "tabshifted");
  } else {
    win.openUILinkIn(url, "tab");
  }
}

module.exports = MDNLink;
