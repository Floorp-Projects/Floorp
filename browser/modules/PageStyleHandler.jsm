/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["PageStyleHandler"];

var PageStyleHandler = {
  getViewer(content) {
    return content.document.docShell.contentViewer;
  },

  sendStyleSheetInfo(mm) {
    let content = mm.content;
    let filteredStyleSheets = this._filterStyleSheets(this.getAllStyleSheets(content), content);

    mm.sendAsyncMessage("PageStyle:StyleSheets", {
      filteredStyleSheets,
      authorStyleDisabled: this.getViewer(content).authorStyleDisabled,
      preferredStyleSheetSet: content.document.preferredStyleSheetSet
    });
  },

  getAllStyleSheets(frameset) {
    let selfSheets = Array.slice(frameset.document.styleSheets);
    let subSheets = Array.map(frameset.frames, frame => this.getAllStyleSheets(frame));
    return selfSheets.concat(...subSheets);
  },

  receiveMessage(msg) {
    let content = msg.target.content;
    switch (msg.name) {
      case "PageStyle:Switch":
        this.getViewer(content).authorStyleDisabled = false;
        this._stylesheetSwitchAll(content, msg.data.title);
        break;

      case "PageStyle:Disable":
        this.getViewer(content).authorStyleDisabled = true;
        break;
    }

    this.sendStyleSheetInfo(msg.target);
  },

  handleEvent(event) {
    let win = event.target.ownerGlobal;
    if (win != win.top) {
      return;
    }

    let mm = win.document.docShell
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIContentFrameMessageManager);
    this.sendStyleSheetInfo(mm);
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

  _filterStyleSheets(styleSheets, content) {
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
