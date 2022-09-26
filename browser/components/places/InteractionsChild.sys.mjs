/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

ChromeUtils.defineESModuleGetters(lazy, {
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.sys.mjs",
});

/**
 * Listens for interactions in the child process and passes information to the
 * parent.
 */
export class InteractionsChild extends JSWindowActorChild {
  #progressListener;
  #currentURL;

  actorCreated() {
    this.isContentWindowPrivate = lazy.PrivateBrowsingUtils.isContentWindowPrivate(
      this.contentWindow
    );

    if (this.isContentWindowPrivate) {
      return;
    }

    this.#progressListener = {
      onLocationChange: (webProgress, request, location, flags) => {
        this.onLocationChange(webProgress, request, location, flags);
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener2",
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };

    let webProgress = this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      this.#progressListener,
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
        Ci.nsIWebProgress.NOTIFY_LOCATION
    );
  }

  didDestroy() {
    // If the tab is closed then the docshell is no longer available.
    if (!this.#progressListener || !this.docShell) {
      return;
    }

    let webProgress = this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this.#progressListener);
  }

  onLocationChange(webProgress, request, location, flags) {
    // We don't care about inner-frame navigations.
    if (!webProgress.isTopLevel) {
      return;
    }

    // If this is a new document then the DOMContentLoaded event will trigger
    // the new interaction instead.
    if (!(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
      return;
    }

    this.#recordNewPage();
  }

  #recordNewPage() {
    let docInfo = this.#getDocumentInfo();
    if (!docInfo || !this.docShell.currentDocumentChannel) {
      // If there is no document channel, then it is something we're not
      // interested in, but we do need to know that the previous interaction
      // has ended.
      this.sendAsyncMessage("Interactions:PageHide");
      return;
    }

    // This may happen when the page calls replaceState or pushState with the
    // same URL. We'll just consider this to not be a new page.
    if (docInfo.url == this.#currentURL) {
      return;
    }

    this.#currentURL = docInfo.url;

    if (
      this.docShell.currentDocumentChannel instanceof Ci.nsIHttpChannel &&
      !this.docShell.currentDocumentChannel.requestSucceeded
    ) {
      return;
    }

    this.sendAsyncMessage("Interactions:PageLoaded", docInfo);
  }

  async handleEvent(event) {
    if (this.isContentWindowPrivate) {
      // No recording in private browsing mode.
      return;
    }
    switch (event.type) {
      case "DOMContentLoaded": {
        this.#recordNewPage();
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

    let requirements = lazy.InteractionsBlocklist.urlRequirements.get(
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
