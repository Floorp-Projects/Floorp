/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import Services from "devtools-services";
import { addThreadEventListeners } from "./events";

/**
 * Temporary helper to check if the current server will support a call to
 * listWorkers. On Fennec 60 or older, the call will silently crash and prevent
 * the client from resuming.
 * XXX: Remove when FF60 for Android is no longer used or available.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1443550 for more details.
 */
export async function checkServerSupportsListWorkers({
  tabTarget,
  debuggerClient
}: Object) {
  const root = await tabTarget.root;
  // root is not available on all debug targets.
  if (!root) {
    return false;
  }

  const deviceFront = await debuggerClient.mainRoot.getFront("device");
  const description = await deviceFront.getDescription();

  const isFennec = description.apptype === "mobile/android";
  if (!isFennec) {
    // Explicitly return true early to avoid calling Services.vs.compare.
    // This would force us to extent the Services shim provided by
    // devtools-modules, used when this code runs in a tab.
    return true;
  }

  // We are only interested in Fennec release versions here.
  // We assume that the server fix for Bug 1443550 will land in FF61.
  const version = description.platformversion;
  return Services.vc.compare(version, "61.0") >= 0;
}

export async function updateWorkerClients({
  tabTarget,
  debuggerClient,
  threadClient,
  workerClients
}: Object) {
  const newWorkerClients = {};

  // Temporary workaround for Bug 1443550
  // XXX: Remove when FF60 for Android is no longer used or available.
  const supportsListWorkers = await checkServerSupportsListWorkers({
    tabTarget,
    debuggerClient
  });

  // NOTE: The Worker and Browser Content toolboxes do not have a parent
  // with a listWorkers function
  // TODO: there is a listWorkers property, but it is not a function on the
  // parent. Investigate what it is
  if (
    !threadClient._parent ||
    typeof threadClient._parent.listWorkers != "function" ||
    !supportsListWorkers
  ) {
    return newWorkerClients;
  }

  const { workers } = await threadClient._parent.listWorkers();
  for (const workerTargetFront of workers) {
    await workerTargetFront.attach();
    const [, workerThread] = await workerTargetFront.attachThread();

    if (workerClients[workerThread.actor]) {
      if (workerClients[workerThread.actor].thread != workerThread) {
        throw new Error(`Multiple clients for actor ID: ${workerThread.actor}`);
      }
      newWorkerClients[workerThread.actor] = workerClients[workerThread.actor];
    } else {
      addThreadEventListeners(workerThread);
      workerThread.url = workerTargetFront.url;
      workerThread.resume();

      const [, consoleClient] = await debuggerClient.attachConsole(
        workerTargetFront.targetForm.consoleActor,
        []
      );

      newWorkerClients[workerThread.actor] = {
        url: workerTargetFront.url,
        thread: workerThread,
        console: consoleClient
      };
    }
  }

  return newWorkerClients;
}
