/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["InteractionsChild"];

/**
 * Listens for interactions in the child process and passes information to the
 * parent.
 */
class InteractionsChild extends JSWindowActorChild {
  async handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded": {
        if (
          !this.docShell.currentDocumentChannel ||
          !(this.docShell.currentDocumentChannel instanceof Ci.nsIHttpChannel)
        ) {
          return;
        }

        if (!this.docShell.currentDocumentChannel.requestSucceeded) {
          return;
        }

        let docInfo = await this.#getDocumentInfo();
        if (docInfo) {
          this.sendAsyncMessage("Interactions:PageLoaded", docInfo);
        }
        break;
      }
      case "pagehide": {
        if (
          !this.docShell.currentDocumentChannel ||
          !(this.docShell.currentDocumentChannel instanceof Ci.nsIHttpChannel)
        ) {
          return;
        }

        if (!this.docShell.currentDocumentChannel.requestSucceeded) {
          return;
        }

        let docInfo = await this.#getDocumentInfo();
        if (docInfo) {
          this.sendAsyncMessage("Interactions:PageHide");
        }
        break;
      }
    }
  }

  /**
   * Returns the current document information for sending to the parent process.
   *
   * @returns {object|null} [docInfo]
   * @returns {string} docInfo.url
   *   The url of the document.
   */
  async #getDocumentInfo() {
    let doc = this.document;
    if (
      doc.documentURIObject.scheme != "http" &&
      doc.documentURIObject.scheme != "https"
    ) {
      return null;
    }
    return {
      isActive: this.manager.browsingContext.isActive,
      url: doc.documentURIObject.specIgnoringRef,
    };
  }
}
