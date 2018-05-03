/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const { div, button, p } = require("devtools/client/shared/vendor/react-dom-factories");
const { openWebLink } = require("devtools/client/shared/link");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const selectors = require("devtools/client/performance-new/store/selectors");

/**
 * This component provides a helpful description for what is going on in the component
 * and provides some external links.
 */
class Description extends PureComponent {
  static get propTypes() {
    return {
      // StateProps:
      toolbox: PropTypes.object.isRequired
    };
  }

  constructor(props) {
    super(props);
    this.handleLinkClick = this.handleLinkClick.bind(this);
  }

  handleLinkClick(event) {
    openWebLink(event.target.value);
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
        onClick: this.handleLinkClick
      },
      text
    );
  }

  render() {
    return div(
      { className: "perf-description" },
      p(null,
        "This new recording panel is a bit different from the existing " +
          "performance panel. It records the entire browser, and then opens up " +
          "and shares the profile with ",
        this.renderLink(
          "https://perf-html.io",
          "perf-html.io"
        ),
        ", a Mozilla performance analysis tool."
      ),
      p(null,
        "This is still a prototype. Join along or file bugs at: ",
        this.renderLink(
          "https://github.com/devtools-html/perf.html",
          "github.com/devtools-html/perf.html"
        ),
        "."
      )
    );
  }
}

const mapStateToProps = state => ({
  toolbox: selectors.getToolbox(state)
});

module.exports = connect(mapStateToProps)(Description);
