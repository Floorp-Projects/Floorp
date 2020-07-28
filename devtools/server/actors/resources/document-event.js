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
class DocumentEventWatcher {
  constructor(targetActor, { onAvailable }) {
    if (isWorker) {
      return;
    }

    const onDocumentEvent = (name, time) => {
      onAvailable([
        {
          resourceType: DOCUMENT_EVENT,
          name,
          time,
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
