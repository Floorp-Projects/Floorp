/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ResizableViewport = createFactory(require("./ResizableViewport"));

const Types = require("../types");

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

    const viewportSize = window.getViewportSize();
    // The viewport may not have been created yet. Default to justify-content: center
    // for the container.
    let justifyContent = "center";

    // If the RDM viewport is bigger than the window's inner width, set the container's
    // justify-content to start so that the left-most viewport is visible when there's
    // horizontal overflow. That is when the horizontal space become smaller than the
    // viewports and a scrollbar appears, then the first viewport will still be visible.
    if (viewportSize && viewportSize.width > window.innerWidth) {
      justifyContent = "start";
    }

    return (
      dom.div(
        {
          id: "viewports-container",
          style: {
            justifyContent,
          },
        },
        dom.div({ id: "viewports" },
          viewports.map((viewport, i) => {
            return ResizableViewport({
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
        )
      )
    );
  }
}

module.exports = Viewports;
