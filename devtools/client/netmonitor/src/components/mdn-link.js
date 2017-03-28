/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { gDevTools } = require("devtools/client/framework/devtools");
const { L10N } = require("../utils/l10n");

const { a } = DOM;

const LEARN_MORE = L10N.getStr("netmonitor.headers.learnMore");

function MDNLink({ url }) {
  return (
    a({
      className: "learn-more-link",
      title: url,
      onClick: (e) => onLearnMoreClick(e, url),
    }, `[${LEARN_MORE}]`)
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
  if (e.button === 1) {
    win.openUILinkIn(url, "tabshifted");
  } else {
    win.openUILinkIn(url, "tab");
  }
}

module.exports = MDNLink;
