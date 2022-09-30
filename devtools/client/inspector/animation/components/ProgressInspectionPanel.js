/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const TickLabels = createFactory(
  require("resource://devtools/client/inspector/animation/components/TickLabels.js")
);
const TickLines = createFactory(
  require("resource://devtools/client/inspector/animation/components/TickLines.js")
);

/**
 * This component is a panel for the progress of animations or keyframes, supports
 * displaying the ticks, take the areas of indicator and the list.
 */
class ProgressInspectionPanel extends PureComponent {
  static get propTypes() {
    return {
      indicator: PropTypes.any.isRequired,
      list: PropTypes.any.isRequired,
      ticks: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  render() {
    const { indicator, list, ticks } = this.props;

    return dom.div(
      {
        className: "progress-inspection-panel",
      },
      dom.div({ className: "background" }, TickLines({ ticks })),
      dom.div({ className: "indicator" }, indicator),
      dom.div({ className: "header devtools-toolbar" }, TickLabels({ ticks })),
      list
    );
  }
}

module.exports = ProgressInspectionPanel;
