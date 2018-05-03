/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { gDevTools } = require("devtools/client/framework/devtools");

const { a } = dom;

function MDNLink({ url, title }) {
  return (
    a({
      className: "devtools-button learn-more-link",
      title,
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
  let where = "tab";
  if (button === 1 || (button === 0 && (isOSX ? metaKey : ctrlKey))) {
    where = "tabshifted";
  }
  win.openWebLinkIn(url, where, {triggeringPrincipal: win.document.nodePrincipal});
}

module.exports = MDNLink;
