/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

module.exports = async function({ targetFront, onAvailable }) {
  const reflowFront = await targetFront.getFront("reflow");
  reflowFront.on("reflows", reflows =>
    onAvailable([
      {
        resourceType: ResourceCommand.TYPES.REFLOW,
        reflows,
      },
    ])
  );
  await reflowFront.start();
};
