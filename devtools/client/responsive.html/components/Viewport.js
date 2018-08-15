/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");
const ResizableViewport = createFactory(require("./ResizableViewport"));
const ViewportDimension = createFactory(require("./ViewportDimension"));

class Viewport extends Component {
  static get propTypes() {
    return {
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      swapAfterMount: PropTypes.bool.isRequired,
      viewport: PropTypes.shape(Types.viewport).isRequired,
      onBrowserMounted: PropTypes.func.isRequired,
      onContentResize: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onRemoveDeviceAssociation = this.onRemoveDeviceAssociation.bind(this);
    this.onResizeViewport = this.onResizeViewport.bind(this);
  }

  onRemoveDeviceAssociation() {
    const {
      viewport,
      onRemoveDeviceAssociation,
    } = this.props;

    onRemoveDeviceAssociation(viewport.id);
  }

  onResizeViewport(width, height) {
    const {
      viewport,
      onResizeViewport,
    } = this.props;

    onResizeViewport(viewport.id, width, height);
  }

  render() {
    const {
      screenshot,
      swapAfterMount,
      viewport,
      onBrowserMounted,
      onContentResize,
    } = this.props;

    const {
      onRemoveDeviceAssociation,
      onResizeViewport,
    } = this;

    return dom.div(
      {
        className: "viewport",
      },
      ViewportDimension({
        viewport,
        onChangeSize: onResizeViewport,
        onRemoveDeviceAssociation,
      }),
      ResizableViewport({
        screenshot,
        swapAfterMount,
        viewport,
        onBrowserMounted,
        onContentResize,
        onRemoveDeviceAssociation,
        onResizeViewport,
      })
    );
  }
}

module.exports = Viewport;
