/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* eslint-env browser */

"use strict";

const { createClass, createFactory, PropTypes, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  changeDevice,
  resizeViewport,
  rotateViewport
} = require("./actions/viewports");
const { takeScreenshot } = require("./actions/screenshot");
const Types = require("./types");
const Viewports = createFactory(require("./components/viewports"));
const GlobalToolbar = createFactory(require("./components/global-toolbar"));

let App = createClass({
  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    location: Types.location.isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    screenshot: PropTypes.shape(Types.screenshot).isRequired,
  },

  displayName: "App",

  onBrowserMounted() {
    window.postMessage({ type: "browser-mounted" }, "*");
  },

  onChangeViewportDevice(id, device) {
    this.props.dispatch(changeDevice(id, device));
  },

  onContentResize({ width, height }) {
    window.postMessage({
      type: "content-resize",
      width,
      height,
    }, "*");
  },

  onExit() {
    window.postMessage({ type: "exit" }, "*");
  },

  onResizeViewport(id, width, height) {
    this.props.dispatch(resizeViewport(id, width, height));
  },

  onRotateViewport(id) {
    this.props.dispatch(rotateViewport(id));
  },

  onScreenshot() {
    this.props.dispatch(takeScreenshot());
  },

  render() {
    let {
      devices,
      location,
      screenshot,
      viewports,
    } = this.props;

    let {
      onBrowserMounted,
      onChangeViewportDevice,
      onContentResize,
      onExit,
      onResizeViewport,
      onRotateViewport,
      onScreenshot,
    } = this;

    return dom.div(
      {
        id: "app",
      },
      GlobalToolbar({
        screenshot,
        onExit,
        onScreenshot,
      }),
      Viewports({
        devices,
        location,
        screenshot,
        viewports,
        onBrowserMounted,
        onChangeViewportDevice,
        onContentResize,
        onRotateViewport,
        onResizeViewport,
      })
    );
  },

});

module.exports = connect(state => state)(App);
