/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

module.exports = createClass({
  displayName: "TabTarget",

  propTypes: {
    target: PropTypes.shape({
      icon: PropTypes.string,
      outerWindowID: PropTypes.number.isRequired,
      title: PropTypes.string,
      url: PropTypes.string.isRequired
    }).isRequired
  },

  debug() {
    let { target } = this.props;
    window.open("about:devtools-toolbox?type=tab&id=" + target.outerWindowID);
  },

  render() {
    let { target } = this.props;

    return dom.div({ className: "target-container" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon
      }),
      dom.div({ className: "target" },
        // If the title is empty, display the url instead.
        dom.div({ className: "target-name", title: target.url },
          target.title || target.url)
      ),
      dom.button({
        className: "debug-button",
        onClick: this.debug,
      }, Strings.GetStringFromName("debug"))
    );
  }
});
