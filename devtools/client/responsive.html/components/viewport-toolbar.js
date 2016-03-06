/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes } =
  require("devtools/client/shared/vendor/react");

module.exports = createClass({

  displayName: "ViewportToolbar",

  propTypes: {
    onRotateViewport: PropTypes.func.isRequired,
  },

  render() {
    let {
      onRotateViewport,
    } = this.props;

    return dom.div(
      {
        className: "viewport-toolbar",
      },
      dom.button({
        className: "viewport-rotate-button toolbar-button",
        onClick: onRotateViewport,
      })
    );
  },

});
