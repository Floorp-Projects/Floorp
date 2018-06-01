/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Services = require("Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

class TabTarget extends Component {
  static get propTypes() {
    return {
      connect: PropTypes.object,
      target: PropTypes.shape({
        icon: PropTypes.string,
        outerWindowID: PropTypes.number.isRequired,
        title: PropTypes.string,
        url: PropTypes.string.isRequired
      }).isRequired
    };
  }

  constructor(props) {
    super(props);
    this.debug = this.debug.bind(this);
  }

  debug() {
    const { target, connect } = this.props;
    let url = "about:devtools-toolbox?type=tab&id=" + target.outerWindowID;
    if (connect.type == "REMOTE") {
      const {host, port} = connect.params;
      url += `&host=${encodeURIComponent(host)}&port=${encodeURIComponent(port)}`;
    }
    window.open(url);
  }

  render() {
    const { target } = this.props;

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
}

module.exports = TabTarget;
