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
   * Get the stylesheets that have a title (and thus can be switched) in this
   * webpage.
   *
   * @param content     The window object for the page.
   */
  _collectStyleSheets(content) {
    let result = [];

    // Only stylesheets with a title can act as an alternative stylesheet.
    for (let sheet of content.document.styleSheets) {
      if (!sheet.title) {
        continue;
      }

      // Skip any stylesheets that don't match the screen media type.
      let media = sheet.media.mediaText;
      if (media && !content.matchMedia(media).matches) {
        continue;
      }

      result.push({
        title: sheet.title,
        disabled: sheet.disabled,
      });
    }

    return result;
  }
}
