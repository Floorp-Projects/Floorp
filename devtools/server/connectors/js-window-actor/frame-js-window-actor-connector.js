/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This is an alternative to devtools/server/connectors/frame-connector.js that
 * will use JS window actors to spawn a DebuggerServer in the content process
 * of a remote frame. The communication between the parent process
 * DebuggerServer and the content process DebuggerServer will also rely on
 * JsWindowActors, via a JsWindowActorTransport.
 *
 * See DevToolsFrameChild.jsm and DevToolsFrameParent.jsm for the implementation
 * of the actors.
 *
 * Those actors are registered during the DevTools initial startup in
 * DevToolsStartup.jsm (see method _registerDevToolsJsWindowActors).
 */
function connectToFrameWithJsWindowActor(
  connection,
  browsingContext,
  onDestroy
) {
  const parentActor = browsingContext.currentWindowGlobal.getActor(
    "DevToolsFrame"
  );

  if (onDestroy) {
    parentActor.once("devtools-frame-parent-actor-destroyed", onDestroy);
  }
  return parentActor.connectToFrame(connection);
}
exports.connectToFrameWithJsWindowActor = connectToFrameWithJsWindowActor;
