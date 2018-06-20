/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { a } = dom;

loader.lazyRequireGetter(this, "openDocLink", "devtools/client/shared/link", true);

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
  openDocLink(url);
}

module.exports = MDNLink;
