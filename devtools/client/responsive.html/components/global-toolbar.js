/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("../utils/l10n");
const { DOM: dom, createClass, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");
const Types = require("../types");

module.exports = createClass({
  displayName: "GlobalToolbar",

  propTypes: {
    screenshot: PropTypes.shape(Types.screenshot).isRequired,
    touchSimulation: PropTypes.shape(Types.touchSimulation).isRequired,
    onExit: PropTypes.func.isRequired,
    onScreenshot: PropTypes.func.isRequired,
    onUpdateTouchSimulationEnabled: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let {
      screenshot,
      touchSimulation,
      onExit,
      onScreenshot,
      onUpdateTouchSimulationEnabled
    } = this.props;

    let touchButtonClass = "toolbar-button devtools-button";
    if (touchSimulation.enabled) {
      touchButtonClass += " active";
    }

    return dom.header(
      {
        id: "global-toolbar",
        className: "container",
      },
      dom.span(
        {
          className: "title",
        },
        getStr("responsive.title")),
      dom.button({
        id: "global-touch-simulation-button",
        className: touchButtonClass,
        onClick: onUpdateTouchSimulationEnabled,
      }),
      dom.button({
        id: "global-screenshot-button",
        className: "toolbar-button devtools-button",
        title: getStr("responsive.screenshot"),
        onClick: onScreenshot,
        disabled: screenshot.isCapturing,
      }),
      dom.button({
        id: "global-exit-button",
        className: "toolbar-button devtools-button",
        title: getStr("responsive.exit"),
        onClick: onExit,
      })
    );
  },

});
