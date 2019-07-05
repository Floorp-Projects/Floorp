/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  div,
  button,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");
const { openDocLink } = require("devtools/client/shared/link");

/**
 * This component provides a helpful description for what is going on in the component
 * and provides some external links.
 */
class Description extends PureComponent {
  static get propTypes() {
    return {};
  }

  constructor(props) {
    super(props);
    this.handleLinkClick = this.handleLinkClick.bind(this);
  }

  handleLinkClick(event) {
    openDocLink(event.target.value);
  }

  /**
   * Implement links as buttons to avoid any risk of loading the link in the
   * the panel.
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
        "This new recording panel is a bit different from the existing " +
          "performance panel. It records the entire browser, and then opens up " +
          "and shares the profile with ",
        this.renderLink("https://profiler.firefox.com", "profiler.firefox.com"),
        ", a Mozilla performance analysis tool."
      ),
      p(
        null,
        "This is still a prototype. Join along or file bugs at: ",
        this.renderLink(
          "https://github.com/firefox-devtools/profiler",
          "github.com/firefox-devtools/profiler"
        ),
        "."
      )
    );
  }
}

module.exports = Description;
