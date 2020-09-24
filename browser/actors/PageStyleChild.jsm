/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["PageStyleChild"];

class PageStyleChild extends JSWindowActorChild {
  handleEvent(event) {
    // On page show, tell the parent all of the stylesheets this document has.
    if (event.type == "pageshow") {
      // If we are in the topmost browsing context,
      // delete the stylesheets from the previous page.
      if (this.browsingContext.top === this.browsingContext) {
        this.sendAsyncMessage("PageStyle:Clear");
      }

      let window = event.target.ownerGlobal;
      window.requestIdleCallback(() => {
        if (!window || window.closed) {
          return;
        }
        let styleSheets = Array.from(this.document.styleSheets);
        let filteredStyleSheets = this._filterStyleSheets(styleSheets, window);

        this.sendAsyncMessage("PageStyle:Add", {
          filteredStyleSheets,
          authorStyleDisabled: this.docShell.contentViewer.authorStyleDisabled,
          preferredStyleSheetSet: this.document.preferredStyleSheetSet,
        });
      });
    }
  }

  receiveMessage(msg) {
    switch (msg.name) {
      // Sent when the page's enabled style sheet is changed.
      case "PageStyle:Switch":
        this.docShell.contentViewer.authorStyleDisabled = false;
        this._switchStylesheet(msg.data.title);
        break;
      // Sent when "No Style" is chosen.
      case "PageStyle:Disable":
        this.docShell.contentViewer.authorStyleDisabled = true;
        break;
    }
  }

  /**
   * Switch the stylesheet so that only the sheet with the given title is enabled.
   */
  _switchStylesheet(title) {
    let docStyleSheets = this.document.styleSheets;

    // Does this doc contain a stylesheet with this title?
    // If not, it's a subframe's stylesheet that's being changed,
    // so no need to disable stylesheets here.
    let docContainsStyleSheet = !title;
    if (title) {
      for (let docStyleSheet of docStyleSheets) {
        if (docStyleSheet.title === title) {
          docContainsStyleSheet = true;
          break;
        }
      }
    }

    for (let docStyleSheet of docStyleSheets) {
      if (docStyleSheet.title) {
        if (docContainsStyleSheet) {
          docStyleSheet.disabled = docStyleSheet.title !== title;
        }
      } else if (docStyleSheet.disabled) {
        docStyleSheet.disabled = false;
      }
    }
  }

  /**
   * Filter the stylesheets that actually apply to this webpage.
   * @param styleSheets The list of stylesheets from the document.
   * @param content     The window object that the webpage lives in.
   */
  _filterStyleSheets(styleSheets, content) {
    let result = [];

    // Only stylesheets with a title can act as an alternative stylesheet.
    for (let currentStyleSheet of styleSheets) {
      if (!currentStyleSheet.title) {
        continue;
      }

      // Skip any stylesheets that don't match the screen media type.
      if (currentStyleSheet.media.length) {
        let mediaQueryList = currentStyleSheet.media.mediaText;
        if (!content.matchMedia(mediaQueryList).matches) {
          continue;
        }
      }

      let URI;
      try {
        if (
          !currentStyleSheet.ownerNode ||
          // Special-case style nodes, which have no href.
          currentStyleSheet.ownerNode.nodeName.toLowerCase() != "style"
        ) {
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
      let sentURI = !URI || URI.scheme == "data" ? null : URI.spec;

      result.push({
        title: currentStyleSheet.title,
        disabled: currentStyleSheet.disabled,
        href: sentURI,
      });
    }

    return result;
  }
}
