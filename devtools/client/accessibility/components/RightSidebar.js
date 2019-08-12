/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

const { L10N } = require("../utils/l10n");
const Accessible = createFactory(require("./Accessible"));
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const Checks = createFactory(require("./Checks"));

// Component that is responsible for rendering accessible panel's sidebar.
class RightSidebar extends Component {
  static get propTypes() {
    return {
      accessibilityWalker: PropTypes.object.isRequired,
      getDOMWalker: PropTypes.func.isRequired,
    };
  }

  /**
   * Render the sidebar component.
   * @returns Sidebar React component.
   */
  render() {
    const propertiesHeaderID = "accessibility-properties-header";
    const checksHeaderID = "accessibility-checks-header";
    const { accessibilityWalker, getDOMWalker } = this.props;
    return div(
      {
        className: "right-sidebar",
        role: "presentation",
      },
      Accordion({
        items: [
          {
            className: "checks",
            component: Checks({ labelledby: checksHeaderID }),
            header: L10N.getStr("accessibility.checks"),
            labelledby: checksHeaderID,
            opened: true,
          },
          {
            className: "accessible",
            component: Accessible({
              accessibilityWalker,
              getDOMWalker,
              labelledby: propertiesHeaderID,
            }),
            header: L10N.getStr("accessibility.properties"),
            labelledby: propertiesHeaderID,
            opened: true,
          },
        ],
      })
    );
  }
}

module.exports = RightSidebar;
