/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "./chrome/commands";
import { setupEvents, clientEvents, pageEvents } from "./chrome/events";

export async function onConnect(connection: any, actions: Object): Object {
  const {
    tabConnection,
    connTarget: { type },
  } = connection;
  const { Debugger, Runtime, Page } = tabConnection;

  Debugger.enable();
  Debugger.setPauseOnExceptions({ state: "none" });
  Debugger.setAsyncCallStackDepth({ maxDepth: 0 });

  if (type == "chrome") {
    Page.frameNavigated(pageEvents.frameNavigated);
    Page.frameStartedLoading(pageEvents.frameStartedLoading);
    Page.frameStoppedLoading(pageEvents.frameStoppedLoading);
  }

  Debugger.scriptParsed(clientEvents.scriptParsed);
  Debugger.scriptFailedToParse(clientEvents.scriptFailedToParse);
  Debugger.paused(clientEvents.paused);
  Debugger.resumed(clientEvents.resumed);

  setupCommands({ Debugger, Runtime, Page });
  setupEvents({ actions, Page, type, Runtime });
  return {};
}

export { clientCommands, clientEvents };
