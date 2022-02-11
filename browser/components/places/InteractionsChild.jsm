/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["InteractionsChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.jsm",
});

/**
 * Listens for interactions in the child process and passes information to the
 * parent.
 */
class InteractionsChild extends JSWindowActorChild {
  actorCreated() {
    this.isContentWindowPrivate = PrivateBrowsingUtils.isContentWindowPrivate(
      this.contentWindow
    );
  }

  async handleEvent(event) {
    if (this.isContentWindowPrivate) {
      // No recording in private browsing mode.
      return;
    }
    switch (event.type) {
      case "DOMContentLoaded": {
        let docInfo = this.#getDocumentInfo();
        if (!docInfo || !this.docShell.currentDocumentChannel) {
          // If there is no document channel, then it is something we're not
          // interested in, but we do need to know that the previous interaction
          // has ended.
          this.sendAsyncMessage("Interactions:PageHide");
          return;
        }

        if (
          this.docShell.currentDocumentChannel instanceof Ci.nsIHttpChannel &&
          !this.docShell.currentDocumentChannel.requestSucceeded
        ) {
          return;
        }

        this.sendAsyncMessage("Interactions:PageLoaded", docInfo);
        break;
      }
      case "pagehide": {
        if (!this.docShell.currentDocumentChannel) {
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
  #getDocumentInfo() {
    let doc = this.document;

    let requirements = InteractionsBlocklist.urlRequirements.get(
      doc.documentURIObject.scheme + ":"
    );
    if (
      !requirements ||
      (requirements.extension &&
        !doc.documentURIObject.spec.endsWith(requirements.extension))
    ) {
      return null;
    }
    let referrer;
    if (doc.referrer) {
      referrer = Services.io.newURI(doc.referrer);
    }
    return {
      isActive: this.manager.browsingContext.isActive,
      url: doc.documentURIObject.specIgnoringRef,
      referrer: referrer?.specIgnoringRef,
    };
  }
}
