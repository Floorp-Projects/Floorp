/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");

module.exports = createClass({

  displayName: "Browser",

  propTypes: {
    location: Types.location.isRequired,
    width: Types.viewport.width.isRequired,
    height: Types.viewport.height.isRequired,
    isResizing: PropTypes.bool.isRequired,
  },

  render() {
    let {
      location,
      width,
      height,
      isResizing,
    } = this.props;

    let className = "browser";
    if (isResizing) {
      className += " resizing";
    }

    return dom.iframe(
      {
        className,
        src: location,
        width,
        height,
      }
    );
  },

});
