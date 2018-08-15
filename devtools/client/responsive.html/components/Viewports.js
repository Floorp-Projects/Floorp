/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");
const Viewport = createFactory(require("./Viewport"));

class Viewports extends Component {
  static get propTypes() {
    return {
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
      onBrowserMounted: PropTypes.func.isRequired,
      onContentResize: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      screenshot,
      viewports,
      onBrowserMounted,
      onContentResize,
      onRemoveDeviceAssociation,
      onResizeViewport,
    } = this.props;

    return dom.div(
      {
        id: "viewports",
      },
      viewports.map((viewport, i) => {
        return Viewport({
          key: viewport.id,
          screenshot,
          swapAfterMount: i == 0,
          viewport,
          onBrowserMounted,
          onContentResize,
          onRemoveDeviceAssociation,
          onResizeViewport,
        });
      })
    );
  }
}

module.exports = Viewports;
