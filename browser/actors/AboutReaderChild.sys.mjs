/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutReader: "resource://gre/modules/AboutReader.sys.mjs",
  ReaderMode: "resource://gre/modules/ReaderMode.sys.mjs",
  Readerable: "resource://gre/modules/Readerable.sys.mjs",
});

var gUrlsToDocContentType = new Map();
var gUrlsToDocTitle = new Map();

export class AboutReaderChild extends JSWindowActorChild {
  constructor() {
    super();

    this._reader = null;
    this._articlePromise = null;
    this._isLeavingReaderableReaderMode = false;
  }

  didDestroy() {
    this.cancelPotentialPendingReadabilityCheck();
    this.readerModeHidden();
  }

  readerModeHidden() {
    if (this._reader) {
      this._reader.clearActor();
    }
    this._reader = null;
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "Reader:ToggleReaderMode":
        if (!this.isAboutReader) {
          gUrlsToDocContentType.set(
            this.document.URL,
            this.document.contentType
          );
          gUrlsToDocTitle.set(this.document.URL, this.document.title);
          this._articlePromise = lazy.ReaderMode.parseDocument(
            this.document
          ).catch(console.error);

          // Get the article data and cache it in the parent process. The reader mode
          // page will retrieve it when it has loaded.
          let article = await this._articlePromise;
          this.sendAsyncMessage("Reader:EnterReaderMode", article);
        } else {
          this.closeReaderMode();
        }
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
      case "Reader:EnterReaderMode": {
        lazy.ReaderMode.enterReaderMode(this.docShell, this.contentWindow);
        break;
      }
      case "Reader:LeaveReaderMode": {
        lazy.ReaderMode.leaveReaderMode(this.docShell, this.contentWindow);
        break;
      }
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
          let url = this.document.documentURI;
          if (!this._articlePromise) {
            url = decodeURIComponent(url.substr("about:reader?url=".length));
            this._articlePromise = this.sendQuery("Reader:GetCachedArticle", {
              url,
            });
          }
          // Update the toolbar icon to show the "reader active" icon.
          this.sendAsyncMessage("Reader:UpdateReaderButton");
          let docContentType =
            gUrlsToDocContentType.get(url) === "text/plain"
              ? "text/plain"
              : "document";

          let docTitle = gUrlsToDocTitle.get(url);
          this._reader = new lazy.AboutReader(
            this,
            this._articlePromise,
            docContentType,
            docTitle
          );
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
        this._isLeavingReaderableReaderMode = false;
        break;

      case "pageshow":
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case.
        if (aEvent.persisted && this.canDoReadabilityCheck()) {
          this.performReadabilityCheckNow();
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
    if (!this.canDoReadabilityCheck()) {
      return;
    }

    this.scheduleReadabilityCheckPostPaint(forceNonArticle);
  }

  canDoReadabilityCheck() {
    return (
      lazy.Readerable.isEnabledForParseOnLoad &&
      !this.isAboutReader &&
      this.contentWindow &&
      this.contentWindow.windowRoot &&
      this.contentWindow.HTMLDocument.isInstance(this.document) &&
      !this.document.mozSyntheticDocument
    );
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

    this.performReadabilityCheckNow(forceNonArticle);
  }

  performReadabilityCheckNow(forceNonArticle) {
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
    if (
      lazy.Readerable.shouldCheckUri(document.baseURIObject, true) &&
      lazy.Readerable.isProbablyReaderable(document)
    ) {
      this.sendAsyncMessage("Reader:UpdateReaderButton", {
        isArticle: true,
      });
    } else if (forceNonArticle) {
      this.sendAsyncMessage("Reader:UpdateReaderButton", {
        isArticle: false,
      });
    }
  }

  closeReaderMode() {
    if (this.isAboutReader) {
      this._isLeavingReaderableReaderMode = this.isReaderableAboutReader;
      this.sendAsyncMessage("Reader:LeaveReaderMode", {});
    }
  }
}
