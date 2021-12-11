/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

const { L10N } = require("devtools/client/accessibility/utils/l10n");
const Accessible = createFactory(
  require("devtools/client/accessibility/components/Accessible")
);
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const Checks = createFactory(
  require("devtools/client/accessibility/components/Checks")
);

// Component that is responsible for rendering accessible panel's sidebar.
function RightSidebar({ highlightAccessible, unhighlightAccessible, toolbox }) {
  const propertiesID = "accessibility-properties";
  const checksID = "accessibility-checks";

  return div(
    {
      className: "right-sidebar",
      role: "presentation",
      tabIndex: "-1",
    },
    Accordion({
      items: [
        {
          className: "checks",
          component: Checks,
          componentProps: { labelledby: `${checksID}-header` },
          header: L10N.getStr("accessibility.checks"),
          id: checksID,
          opened: true,
        },
        {
          className: "accessible",
          component: Accessible,
          componentProps: {
            highlightAccessible,
            unhighlightAccessible,
            toolboxHighlighter: toolbox.getHighlighter(),
            toolbox,
            labelledby: `${propertiesID}-header`,
          },
          header: L10N.getStr("accessibility.properties"),
          id: propertiesID,
          opened: true,
        },
      ],
    })
  );
}

module.exports = RightSidebar;
