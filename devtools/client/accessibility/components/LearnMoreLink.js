/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  p,
  a,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const { openDocLink } = require("resource://devtools/client/shared/link.js");

/**
 * Localization friendly component for rendering a block of text with a "Learn more" link
 * as a part of it.
 */
class LearnMoreLink extends Component {
  static get propTypes() {
    return {
      className: PropTypes.string,
      href: PropTypes.string,
      learnMoreStringKey: PropTypes.string.isRequired,
      l10n: PropTypes.object.isRequired,
      messageStringKey: PropTypes.string.isRequired,
      onClick: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      href: "#",
      learnMoreStringKey: null,
      l10n: null,
      messageStringKey: null,
      onClick: LearnMoreLink.openDocOnClick,
    };
  }

  static openDocOnClick(event) {
    event.preventDefault();
    openDocLink(event.target.href);
  }

  render() {
    const {
      className,
      href,
      learnMoreStringKey,
      l10n,
      messageStringKey,
      onClick,
    } = this.props;
    const learnMoreString = l10n.getStr(learnMoreStringKey);
    const messageString = l10n.getFormatStr(messageStringKey, learnMoreString);

    // Split the paragraph string with the link as a separator, and include the link into
    // results.
    const re = new RegExp(`(\\b${learnMoreString}\\b)`);
    const contents = messageString.split(re);
    contents[1] = a({ className: "link", href, onClick }, contents[1]);

    return p(
      {
        className,
      },
      ...contents
    );
  }
}

module.exports = LearnMoreLink;
