/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {{}} Props - This is an empty object.
 */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  div,
  button,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * This component provides a helpful description for what is going on in the component
 * and provides some external links.
 * @extends {React.PureComponent<Props>}
 */
class Description extends PureComponent {
  /**
   * @param {Props} props
   */
  constructor(props) {
    super(props);
    this.handleLinkClick = this.handleLinkClick.bind(this);
  }

  /**
   * @param {React.MouseEvent<HTMLButtonElement>} event
   */
  handleLinkClick(event) {
    const { openDocLink } = require("devtools/client/shared/link");

    /** @type HTMLButtonElement */
    const target = /** @type {any} */ (event.target);

    openDocLink(target.value, {});
  }

  /**
   * Implement links as buttons to avoid any risk of loading the link in the
   * the panel.
   * @param {string} href
   * @param {string} text
   */
  renderLink(href, text) {
    return button(
      {
        className: "perf-external-link",
        value: href,
        onClick: this.handleLinkClick,
      },
      text
    );
  }

  render() {
    return div(
      { className: "perf-description" },
      p(
        null,
        "Recordings launch ",
        this.renderLink("https://profiler.firefox.com", "profiler.firefox.com"),
        " in a new tab. All data is stored locally, but you can choose to upload it",
        " for sharing."
      )
    );
  }
}

module.exports = Description;
