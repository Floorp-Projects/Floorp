/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");
const Accessible = createFactory(
  require("resource://devtools/client/accessibility/components/Accessible.js")
);
const Accordion = createFactory(
  require("resource://devtools/client/shared/components/Accordion.js")
);
const Checks = createFactory(
  require("resource://devtools/client/accessibility/components/Checks.js")
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
