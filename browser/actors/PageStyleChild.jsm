/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

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
        let filteredStyleSheets = this._collectStyleSheets(window);
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
        if (this.browsingContext.top == this.browsingContext) {
          this.browsingContext.authorStyleDisabledDefault = false;
        }
        this.docShell.contentViewer.authorStyleDisabled = false;
        this._switchStylesheet(msg.data.title);
        break;
      // Sent when "No Style" is chosen.
      case "PageStyle:Disable":
        if (this.browsingContext.top == this.browsingContext) {
          this.browsingContext.authorStyleDisabledDefault = true;
        }
        this.docShell.contentViewer.authorStyleDisabled = true;
        break;
    }
  }

  /**
   * Returns links that would represent stylesheets once loaded.
   */
  _collectLinks(document) {
    let result = [];
    for (let link of document.querySelectorAll("link")) {
      if (link.namespaceURI !== "http://www.w3.org/1999/xhtml") {
        continue;
      }
      let isStyleSheet = Array.from(link.relList).some(
        r => r.toLowerCase() == "stylesheet"
      );
      if (!isStyleSheet) {
        continue;
      }
      if (!link.href) {
        continue;
      }
      result.push(link);
    }
    return result;
  }

  /**
   * Switch the stylesheet so that only the sheet with the given title is enabled.
   */
  _switchStylesheet(title) {
    let document = this.document;
    let docStyleSheets = Array.from(document.styleSheets);
    let links;

    // Does this doc contain a stylesheet with this title?
    // If not, it's a subframe's stylesheet that's being changed,
    // so no need to disable stylesheets here.
    let docContainsStyleSheet = !title;
    if (title) {
      links = this._collectLinks(document);
      docContainsStyleSheet =
        docStyleSheets.some(sheet => sheet.title == title) ||
        links.some(link => link.title == title);
    }

    for (let sheet of docStyleSheets) {
      if (sheet.title) {
        if (docContainsStyleSheet) {
          sheet.disabled = sheet.title !== title;
        }
      } else if (sheet.disabled) {
        sheet.disabled = false;
      }
    }

    // If there's no title, we just need to disable potentially-enabled
    // stylesheets via document.styleSheets, so no need to deal with links
    // there.
    //
    // We don't want to enable <link rel="stylesheet" disabled> without title
    // that were not enabled before.
    if (title) {
      for (let link of links) {
        if (link.title == title && link.disabled) {
          link.disabled = false;
        }
      }
    }
  }

  /**
   * Get the stylesheets that have a title (and thus can be switched) in this
   * webpage.
   *
   * @param content     The window object for the page.
   */
  _collectStyleSheets(content) {
    let result = [];
    let document = content.document;

    for (let sheet of document.styleSheets) {
      let title = sheet.title;
      if (!title) {
        // Sheets without a title are not alternates.
        continue;
      }

      // Skip any stylesheets that don't match the screen media type.
      let media = sheet.media.mediaText;
      if (media && !content.matchMedia(media).matches) {
        continue;
      }

      // We skip links here, see below.
      if (
        sheet.href &&
        sheet.ownerNode &&
        sheet.ownerNode.nodeName.toLowerCase() == "link"
      ) {
        continue;
      }

      let disabled = sheet.disabled;
      result.push({ title, disabled });
    }

    // This is tricky, because we can't just rely on document.styleSheets, as
    // `<link disabled>` makes the sheet don't appear there at all.
    for (let link of this._collectLinks(document)) {
      let title = link.title;
      if (!title) {
        continue;
      }

      let media = link.media;
      if (media && !content.matchMedia(media).matches) {
        continue;
      }

      let disabled =
        link.disabled ||
        !!link.sheet?.disabled ||
        document.preferredStyleSheetSet != title;
      result.push({ title, disabled });
    }

    return result;
  }
}
