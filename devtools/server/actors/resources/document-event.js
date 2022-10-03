/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { DOCUMENT_EVENT },
} = require("resource://devtools/server/actors/resources/index.js");
const {
  DocumentEventsListener,
} = require("resource://devtools/server/actors/webconsole/listeners/document-events.js");

class DocumentEventWatcher {
  #abortController = new AbortController();
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
        // This will be `true` when the user selected a document in the frame picker tool,
        // in the toolbox toolbar.
        isFrameSwitching,
        // This is only passed for dom-complete event
        hasNativeConsoleAPI,
        // This is only passed for will-navigate event
        newURI,
      } = {}
    ) => {
      // Ignore will-navigate as that's managed by parent-process-document-event.js.
      // Except frame switching, when selecting an iframe document via the dropdown menu,
      // this is handled by the target actor in the content process and the parent process
      // doesn't know about it.
      if (name == "will-navigate" && !isFrameSwitching) {
        return;
      }
      onAvailable([
        {
          resourceType: DOCUMENT_EVENT,
          name,
          time,
          isFrameSwitching,
          // only send `title` on dom interactive (once the HTML was parsed) so we don't
          // make the payload bigger for events where we either don't have a title yet,
          // or where we already had a chance to get the title.
          title: name === "dom-interactive" ? targetActor.title : undefined,
          // only send `url` on dom loading and dom-interactive so we don't make the
          // payload bigger for other events
          url:
            name === "dom-loading" || name === "dom-interactive"
              ? targetActor.url
              : undefined,
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

    this.listener.on(
      "will-navigate",
      data => onDocumentEvent("will-navigate", data),
      { signal: this.#abortController.signal }
    );
    this.listener.on(
      "dom-loading",
      data => onDocumentEvent("dom-loading", data),
      { signal: this.#abortController.signal }
    );
    this.listener.on(
      "dom-interactive",
      data => onDocumentEvent("dom-interactive", data),
      { signal: this.#abortController.signal }
    );
    this.listener.on(
      "dom-complete",
      data => onDocumentEvent("dom-complete", data),
      { signal: this.#abortController.signal }
    );

    this.listener.listen();
  }

  destroy() {
    this.#abortController.abort();
    if (this.listener) {
      this.listener.destroy();
    }
  }
}

module.exports = DocumentEventWatcher;
