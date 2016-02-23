/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const ResizableViewport = createFactory(require("./resizable-viewport"));
const ViewportDimension = createFactory(require("./viewport-dimension"));

module.exports = createClass({

  displayName: "Viewport",

  propTypes: {
    location: Types.location.isRequired,
    viewport: PropTypes.shape(Types.viewport).isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onRotateViewport: PropTypes.func.isRequired,
  },

  render() {
    let {
      location,
      viewport,
      onResizeViewport,
      onRotateViewport,
    } = this.props;

    return dom.div(
      {
        className: "viewport",
      },
      ResizableViewport({
        location,
        viewport,
        onResizeViewport,
        onRotateViewport,
      }),
      ViewportDimension({
        viewport,
        onResizeViewport,
      })
    );
  },

});
