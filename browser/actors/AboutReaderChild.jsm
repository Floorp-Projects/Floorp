/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AboutReaderChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "AboutReader",
                               "resource://gre/modules/AboutReader.jsm");
ChromeUtils.defineModuleGetter(this, "ReaderMode",
                               "resource://gre/modules/ReaderMode.jsm");

class AboutReaderChild extends ActorChild {
  constructor(mm) {
    super(mm);

    this._articlePromise = null;
    this._isLeavingReaderableReaderMode = false;
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Reader:ToggleReaderMode":
        if (!this.isAboutReader) {
          this._articlePromise = ReaderMode.parseDocument(this.content.document).catch(Cu.reportError);
          ReaderMode.enterReaderMode(this.mm.docShell, this.content);
        } else {
          this._isLeavingReaderableReaderMode = this.isReaderableAboutReader;
          ReaderMode.leaveReaderMode(this.mm.docShell, this.content);
        }
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
    }
  }

  get isAboutReader() {
    if (!this.content) {
      return false;
    }
    return this.content.document.documentURI.startsWith("about:reader");
  }

  get isReaderableAboutReader() {
    return this.isAboutReader &&
      !this.content.document.documentElement.dataset.isError;
  }

  handleEvent(aEvent) {
    if (aEvent.originalTarget.defaultView != this.content) {
      return;
    }

    switch (aEvent.type) {
      case "AboutReaderContentLoaded":
        if (!this.isAboutReader) {
          return;
        }

        if (this.content.document.body) {
          // Update the toolbar icon to show the "reader active" icon.
          this.mm.sendAsyncMessage("Reader:UpdateReaderButton");
          new AboutReader(this.mm, this.content, this._articlePromise);
          this._articlePromise = null;
        }
        break;

      case "pagehide":
        this.cancelPotentialPendingReadabilityCheck();
        // this._isLeavingReaderableReaderMode is used here to keep the Reader Mode icon
        // visible in the location bar when transitioning from reader-mode page
        // back to the readable source page.
        this.mm.sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: this._isLeavingReaderableReaderMode });
        if (this._isLeavingReaderableReaderMode) {
          this._isLeavingReaderableReaderMode = false;
        }
        break;

      case "pageshow":
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case.
        if (aEvent.persisted) {
          this.updateReaderButton();
        }
        break;
      case "DOMContentLoaded":
        this.updateReaderButton();
        break;

    }
  }

  /**
   * NB: this function will update the state of the reader button asynchronously
   * after the next mozAfterPaint call (assuming reader mode is enabled and
   * this is a suitable document). Calling it on things which won't be
   * painted is not going to work.
   */
  updateReaderButton(forceNonArticle) {
    if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader ||
        !this.content || !(this.content.document instanceof this.content.HTMLDocument) ||
        this.content.document.mozSyntheticDocument) {
      return;
    }

    this.scheduleReadabilityCheckPostPaint(forceNonArticle);
  }

  cancelPotentialPendingReadabilityCheck() {
    if (this._pendingReadabilityCheck) {
      this.mm.removeEventListener("MozAfterPaint", this._pendingReadabilityCheck);
      delete this._pendingReadabilityCheck;
    }
  }

  scheduleReadabilityCheckPostPaint(forceNonArticle) {
    if (this._pendingReadabilityCheck) {
      // We need to stop this check before we re-add one because we don't know
      // if forceNonArticle was true or false last time.
      this.cancelPotentialPendingReadabilityCheck();
    }
    this._pendingReadabilityCheck = this.onPaintWhenWaitedFor.bind(this, forceNonArticle);
    this.mm.addEventListener("MozAfterPaint", this._pendingReadabilityCheck);
  }

  onPaintWhenWaitedFor(forceNonArticle, event) {
    // In non-e10s, we'll get called for paints other than ours, and so it's
    // possible that this page hasn't been laid out yet, in which case we
    // should wait until we get an event that does relate to our layout. We
    // determine whether any of our this.content got painted by checking if there
    // are any painted rects.
    if (!event.clientRects.length) {
      return;
    }

    this.cancelPotentialPendingReadabilityCheck();
    // Only send updates when there are articles; there's no point updating with
    // |false| all the time.
    if (ReaderMode.isProbablyReaderable(this.content.document)) {
      this.mm.sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: true });
    } else if (forceNonArticle) {
      this.mm.sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
    }
  }
}
