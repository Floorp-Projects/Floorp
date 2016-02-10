/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const Browser = createFactory(require("./browser"));
const ViewportToolbar = createFactory(require("./viewport-toolbar"));

module.exports = createClass({

  displayName: "Viewport",

  propTypes: {
    location: Types.location.isRequired,
    viewport: PropTypes.shape(Types.viewport).isRequired,
    onRotateViewport: PropTypes.func.isRequired,
  },

  render() {
    let {
      location,
      viewport,
      onRotateViewport,
    } = this.props;

    return dom.div(
      {
        className: "viewport"
      },
      ViewportToolbar({
        onRotateViewport,
      }),
      Browser({
        location,
        width: viewport.width,
        height: viewport.height,
      })
    );
  },

});
