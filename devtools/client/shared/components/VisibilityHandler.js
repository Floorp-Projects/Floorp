/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper class to disable panel rendering when it is in background.
 *
 * Toolbox code hides the iframes when switching to another panel
 * and triggers `visibilitychange` events.
 *
 * See devtools/client/framework/toolbox.js:setIframeVisible().
 */

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class VisibilityHandler extends Component {
  static get propTypes() {
    return {
      children: PropTypes.element.isRequired
    };
  }

  constructor(props) {
    super(props);

    this.onVisibilityChange = this.onVisibilityChange.bind(this);
  }

  componentDidMount() {
    window.addEventListener("visibilitychange", this.onVisibilityChange);
  }

  shouldComponentUpdate() {
    return document.visibilityState == "visible";
  }

  componentWillUnmount() {
    window.removeEventListener("visibilitychange", this.onVisibilityChange);
  }

  onVisibilityChange() {
    if (document.visibilityState == "visible") {
      this.forceUpdate();
    }
  }

  render() {
    return this.props.children;
  }
}

module.exports = VisibilityHandler;
