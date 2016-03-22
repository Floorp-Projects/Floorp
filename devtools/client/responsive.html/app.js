/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, createFactory, PropTypes, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  changeDevice,
  resizeViewport,
  rotateViewport
} = require("./actions/viewports");
const Types = require("./types");
const Viewports = createFactory(require("./components/viewports"));
const GlobalToolbar = createFactory(require("./components/global-toolbar"));

let App = createClass({

  displayName: "App",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    location: Types.location.isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    onExit: PropTypes.func.isRequired,
  },

  onChangeViewportDevice(id, device) {
    this.props.dispatch(changeDevice(id, device));
  },

  onResizeViewport(id, width, height) {
    this.props.dispatch(resizeViewport(id, width, height));
  },

  onRotateViewport(id) {
    this.props.dispatch(rotateViewport(id));
  },

  render() {
    let {
      devices,
      location,
      viewports,
      onExit,
    } = this.props;

    let {
      onChangeViewportDevice,
      onResizeViewport,
      onRotateViewport,
    } = this;

    return dom.div(
      {
        id: "app",
      },
      GlobalToolbar({
        onExit,
      }),
      Viewports({
        devices,
        location,
        viewports,
        onChangeViewportDevice,
        onRotateViewport,
        onResizeViewport,
      })
    );
  },

});

module.exports = connect(state => state)(App);
