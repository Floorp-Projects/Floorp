/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

const { L10N } = require("../utils/l10n");
const Accessible = createFactory(require("./Accessible"));
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const Checks = createFactory(require("./Checks"));

// Component that is responsible for rendering accessible panel's sidebar.
function RightSidebar() {
  const propertiesID = "accessibility-properties";
  const checksID = "accessibility-checks";

  return div(
    {
      className: "right-sidebar",
      role: "presentation",
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
