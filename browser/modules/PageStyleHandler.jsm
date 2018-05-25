/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var PageStyleHandler = {
  init() {
    addMessageListener("PageStyle:Switch", this);
    addMessageListener("PageStyle:Disable", this);
    addEventListener("pageshow", () => this.sendStyleSheetInfo());
  },

  get markupDocumentViewer() {
    return docShell.contentViewer;
  },

  sendStyleSheetInfo() {
    let filteredStyleSheets = this._filterStyleSheets(this.getAllStyleSheets());

    sendAsyncMessage("PageStyle:StyleSheets", {
      filteredStyleSheets,
      authorStyleDisabled: this.markupDocumentViewer.authorStyleDisabled,
      preferredStyleSheetSet: content.document.preferredStyleSheetSet
    });
  },

  getAllStyleSheets(frameset = content) {
    let selfSheets = Array.slice(frameset.document.styleSheets);
    let subSheets = Array.map(frameset.frames, frame => this.getAllStyleSheets(frame));
    return selfSheets.concat(...subSheets);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "PageStyle:Switch":
        this.markupDocumentViewer.authorStyleDisabled = false;
        this._stylesheetSwitchAll(content, msg.data.title);
        break;

      case "PageStyle:Disable":
        this.markupDocumentViewer.authorStyleDisabled = true;
        break;
    }

    this.sendStyleSheetInfo();
  },

  _stylesheetSwitchAll(frameset, title) {
    if (!title || this._stylesheetInFrame(frameset, title)) {
      this._stylesheetSwitchFrame(frameset, title);
    }

    for (let i = 0; i < frameset.frames.length; i++) {
      // Recurse into sub-frames.
      this._stylesheetSwitchAll(frameset.frames[i], title);
    }
  },

  _stylesheetSwitchFrame(frame, title) {
    var docStyleSheets = frame.document.styleSheets;

    for (let i = 0; i < docStyleSheets.length; ++i) {
      let docStyleSheet = docStyleSheets[i];
      if (docStyleSheet.title) {
        docStyleSheet.disabled = (docStyleSheet.title != title);
      } else if (docStyleSheet.disabled) {
        docStyleSheet.disabled = false;
      }
    }
  },

  _stylesheetInFrame(frame, title) {
    return Array.some(frame.document.styleSheets, (styleSheet) => styleSheet.title == title);
  },

  _filterStyleSheets(styleSheets) {
    let result = [];

    for (let currentStyleSheet of styleSheets) {
      if (!currentStyleSheet.title)
        continue;

      // Skip any stylesheets that don't match the screen media type.
      if (currentStyleSheet.media.length > 0) {
        let mediaQueryList = currentStyleSheet.media.mediaText;
        if (!content.matchMedia(mediaQueryList).matches) {
          continue;
        }
      }

      let URI;
      try {
        if (!currentStyleSheet.ownerNode ||
            // special-case style nodes, which have no href
            currentStyleSheet.ownerNode.nodeName.toLowerCase() != "style") {
          URI = Services.io.newURI(currentStyleSheet.href);
        }
      } catch (e) {
        if (e.result != Cr.NS_ERROR_MALFORMED_URI) {
          throw e;
        }
        continue;
      }

      // We won't send data URIs all of the way up to the parent, as these
      // can be arbitrarily large.
      let sentURI = (!URI || URI.scheme == "data") ? null : URI.spec;

      result.push({
        title: currentStyleSheet.title,
        disabled: currentStyleSheet.disabled,
        href: sentURI,
      });
    }

    return result;
  },
};
PageStyleHandler.init();
