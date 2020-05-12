/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AboutReaderChild"];

ChromeUtils.defineModuleGetter(
  this,
  "AboutReader",
  "resource://gre/modules/AboutReader.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Readerable",
  "resource://gre/modules/Readerable.jsm"
);

class AboutReaderChild extends JSWindowActorChild {
  constructor() {
    super();

    this._reader = null;
    this._articlePromise = null;
    this._isLeavingReaderableReaderMode = false;
  }

  willDestroy() {
    this.cancelPotentialPendingReadabilityCheck();
    this.readerModeHidden();
  }

  readerModeHidden() {
    if (this._reader) {
      this._reader.clearActor();
    }
    this._reader = null;
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Reader:ToggleReaderMode":
        if (!this.isAboutReader) {
          this._articlePromise = ReaderMode.parseDocument(this.document).catch(
            Cu.reportError
          );
          ReaderMode.enterReaderMode(this.docShell, this.contentWindow);
        } else {
          this._isLeavingReaderableReaderMode = this.isReaderableAboutReader;
          ReaderMode.leaveReaderMode(this.docShell, this.contentWindow);
        }
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
    }

    // Forward the message to the reader if it has been created.
    if (this._reader) {
      this._reader.receiveMessage(message);
    }
  }

  get isAboutReader() {
    if (!this.document) {
      return false;
    }
    return this.document.documentURI.startsWith("about:reader");
  }

  get isReaderableAboutReader() {
    return this.isAboutReader && !this.document.documentElement.dataset.isError;
  }

  handleEvent(aEvent) {
    if (aEvent.originalTarget.defaultView != this.contentWindow) {
      return;
    }

    switch (aEvent.type) {
      case "DOMContentLoaded":
        if (!this.isAboutReader) {
          this.updateReaderButton();
          return;
        }

        if (this.document.body) {
          // Update the toolbar icon to show the "reader active" icon.
          this.sendAsyncMessage("Reader:UpdateReaderButton");
          this._reader = new AboutReader(this, this._articlePromise);
          this._articlePromise = null;
        }
        break;

      case "pagehide":
        this.cancelPotentialPendingReadabilityCheck();
        // this._isLeavingReaderableReaderMode is used here to keep the Reader Mode icon
        // visible in the location bar when transitioning from reader-mode page
        // back to the readable source page.
        this.sendAsyncMessage("Reader:UpdateReaderButton", {
          isArticle: this._isLeavingReaderableReaderMode,
        });
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
    }
  }

  /**
   * NB: this function will update the state of the reader button asynchronously
   * after the next mozAfterPaint call (assuming reader mode is enabled and
   * this is a suitable document). Calling it on things which won't be
   * painted is not going to work.
   */
  updateReaderButton(forceNonArticle) {
    if (
      !Readerable.isEnabledForParseOnLoad ||
      this.isAboutReader ||
      !this.contentWindow ||
      !(this.document instanceof this.contentWindow.HTMLDocument) ||
      this.document.mozSyntheticDocument
    ) {
      return;
    }

    this.scheduleReadabilityCheckPostPaint(forceNonArticle);
  }

  cancelPotentialPendingReadabilityCheck() {
    if (this._pendingReadabilityCheck) {
      if (this._listenerWindow) {
        this._listenerWindow.removeEventListener(
          "MozAfterPaint",
          this._pendingReadabilityCheck
        );
      }
      delete this._pendingReadabilityCheck;
      delete this._listenerWindow;
    }
  }

  scheduleReadabilityCheckPostPaint(forceNonArticle) {
    if (this._pendingReadabilityCheck) {
      // We need to stop this check before we re-add one because we don't know
      // if forceNonArticle was true or false last time.
      this.cancelPotentialPendingReadabilityCheck();
    }
    this._pendingReadabilityCheck = this.onPaintWhenWaitedFor.bind(
      this,
      forceNonArticle
    );

    this._listenerWindow = this.contentWindow.windowRoot;
    this.contentWindow.windowRoot.addEventListener(
      "MozAfterPaint",
      this._pendingReadabilityCheck
    );
  }

  onPaintWhenWaitedFor(forceNonArticle, event) {
    // In non-e10s, we'll get called for paints other than ours, and so it's
    // possible that this page hasn't been laid out yet, in which case we
    // should wait until we get an event that does relate to our layout. We
    // determine whether any of our this.contentWindow got painted by checking
    // if there are any painted rects.
    if (!event.clientRects.length) {
      return;
    }

    this.cancelPotentialPendingReadabilityCheck();

    // Ignore errors from actors that have been unloaded before the
    // paint event timer fires.
    let document;
    try {
      document = this.document;
    } catch (ex) {
      return;
    }

    // Only send updates when there are articles; there's no point updating with
    // |false| all the time.
    if (Readerable.isProbablyReaderable(document)) {
      this.sendAsyncMessage("Reader:UpdateReaderButton", {
        isArticle: true,
      });
    } else if (forceNonArticle) {
      this.sendAsyncMessage("Reader:UpdateReaderButton", {
        isArticle: false,
      });
    }
  }
}
