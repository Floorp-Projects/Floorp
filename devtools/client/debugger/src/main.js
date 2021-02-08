/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { onConnect, onDisconnect } from "./client";
import { teardownWorkers } from "./utils/bootstrap";
import sourceQueue from "./utils/source-queue";

module.exports = {
  bootstrap: ({
    targetList,
    resourceWatcher,
    devToolsClient,
    workers,
    panel,
  }: any) =>
    onConnect(
      {
        tab: { clientType: "firefox" },
        targetList,
        resourceWatcher,
        devToolsClient,
      },
      workers,
      panel
    ),
  destroy: () => {
    onDisconnect();
    sourceQueue.clear();
    teardownWorkers();
  },
};
