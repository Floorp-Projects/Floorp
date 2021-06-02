/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { DOCUMENT_EVENT },
} = require("devtools/server/actors/resources/index");
const {
  DocumentEventsListener,
} = require("devtools/server/actors/webconsole/listeners/document-events");

class DocumentEventWatcher {
  /**
   * Start watching for all document event related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe document event
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    if (isWorker) {
      return;
    }

    const onDocumentEvent = (
      name,
      {
        time,
        // This is only passed for dom-loading event
        shouldBeIgnoredAsRedundantWithTargetAvailable,
        // This will be `true` when the user selected a document in the frame picker tool,
        // in the toolbox toolbar.
        isFrameSwitching,
        // This is only passed for dom-complete event
        hasNativeConsoleAPI,
        // This is only passed for will-navigate event
        newURI,
      } = {}
    ) => {
      onAvailable([
        {
          resourceType: DOCUMENT_EVENT,
          name,
          time,
          shouldBeIgnoredAsRedundantWithTargetAvailable,
          isFrameSwitching,
          // only send `title` on dom interactive (once the HTML was parsed) so we don't
          // make the payload bigger for events where we either don't have a title yet,
          // or where we already had a chance to get the title.
          title: name === "dom-interactive" ? targetActor.title : undefined,
          // only send `url` on dom loading so we don't make the payload bigger for
          // other events
          url: name === "dom-loading" ? targetActor.url : undefined,
          // only send `newURI` on will navigate so we don't make the payload bigger for
          // other events
          newURI: name === "will-navigate" ? newURI : null,
          // only send `hasNativeConsoleAPI` on dom complete so we don't make the payload bigger for
          // other events
          hasNativeConsoleAPI:
            name == "dom-complete" ? hasNativeConsoleAPI : null,
        },
      ]);
    };

    this.listener = new DocumentEventsListener(targetActor);
    this.listener.on("*", onDocumentEvent);
    this.listener.listen();
  }

  destroy() {
    if (this.listener) {
      this.listener.destroy();
    }
  }
}

module.exports = DocumentEventWatcher;
