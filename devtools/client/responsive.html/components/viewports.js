/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const Viewport = createFactory(require("./viewport"));

module.exports = createClass({
  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    location: Types.location.isRequired,
    screenshot: PropTypes.shape(Types.screenshot).isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    onBrowserMounted: PropTypes.func.isRequired,
    onChangeViewportDevice: PropTypes.func.isRequired,
    onContentResize: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onRotateViewport: PropTypes.func.isRequired,
  },

  displayName: "Viewports",

  render() {
    let {
      devices,
      location,
      screenshot,
      viewports,
      onBrowserMounted,
      onChangeViewportDevice,
      onContentResize,
      onResizeViewport,
      onRotateViewport,
    } = this.props;

    return dom.div(
      {
        id: "viewports",
      },
      viewports.map(viewport => {
        return Viewport({
          key: viewport.id,
          devices,
          location,
          screenshot,
          viewport,
          onBrowserMounted,
          onChangeViewportDevice,
          onContentResize,
          onResizeViewport,
          onRotateViewport,
        });
      })
    );
  },

});
