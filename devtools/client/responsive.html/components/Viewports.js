/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const ResizableViewport = createFactory(require("./ResizableViewport"));

const Types = require("../types");

class Viewports extends PureComponent {
  static get propTypes() {
    return {
      leftAlignmentEnabled: PropTypes.bool.isRequired,
      onBrowserMounted: PropTypes.func.isRequired,
      onContentResize: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
      doResizeViewport: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    };
  }

  render() {
    const {
      leftAlignmentEnabled,
      onBrowserMounted,
      onContentResize,
      onRemoveDeviceAssociation,
      doResizeViewport,
      onResizeViewport,
      screenshot,
      viewports,
    } = this.props;

    const viewportSize = window.getViewportSize();
    // The viewport may not have been created yet. Default to justify-content: center
    // for the container.
    let justifyContent = "center";

    // If the RDM viewport is bigger than the window's inner width, set the container's
    // justify-content to start so that the left-most viewport is visible when there's
    // horizontal overflow. That is when the horizontal space become smaller than the
    // viewports and a scrollbar appears, then the first viewport will still be visible.
    if (leftAlignmentEnabled ||
        (viewportSize && viewportSize.width > window.innerWidth)) {
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
        dom.div(
          {
            id: "viewports",
            className: leftAlignmentEnabled ? "left-aligned" : "",
          },
          viewports.map((viewport, i) => {
            return ResizableViewport({
              key: viewport.id,
              leftAlignmentEnabled,
              onBrowserMounted,
              onContentResize,
              onRemoveDeviceAssociation,
              doResizeViewport,
              onResizeViewport,
              screenshot,
              swapAfterMount: i == 0,
              viewport,
            });
          })
        )
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    leftAlignmentEnabled: state.ui.leftAlignmentEnabled,
  };
};

module.exports = connect(mapStateToProps)(Viewports);
