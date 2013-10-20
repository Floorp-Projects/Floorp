/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PageStyle"];

const Ci = Components.interfaces;

/**
 * The external API exported by this module.
 */
this.PageStyle = Object.freeze({
  collect: function (docShell) {
    return PageStyleInternal.collect(docShell);
  },

  restore: function (docShell, frameList, pageStyle) {
    PageStyleInternal.restore(docShell, frameList, pageStyle);
  },
});

// Signifies that author style level is disabled for the page.
const NO_STYLE = "_nostyle";

let PageStyleInternal = {
  /**
   * Find out the title of the style sheet selected for the given
   * docshell. Recurse into frames if needed.
   */
  collect: function (docShell) {
    let markupDocumentViewer =
      docShell.contentViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);
    if (markupDocumentViewer.authorStyleDisabled) {
      return NO_STYLE;
    }

    let content = docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);

    return this.collectFrame(content);
  },

  /**
   * Determine the title of the currently enabled style sheet (if any);
   * recurse through the frameset if necessary.
   * @param   content is a frame reference
   * @returns the title style sheet determined to be enabled (empty string if none)
   */
  collectFrame: function (content) {
    const forScreen = /(?:^|,)\s*(?:all|screen)\s*(?:,|$)/i;

    let sheets = content.document.styleSheets;
    for (let i = 0; i < sheets.length; i++) {
      let ss = sheets[i];
      let media = ss.media.mediaText;
      if (!ss.disabled && ss.title && (!media || forScreen.test(media))) {
        return ss.title;
      }
    }

    for (let i = 0; i < content.frames.length; i++) {
      let selectedPageStyle = this.collectFrame(content.frames[i]);
      if (selectedPageStyle) {
        return selectedPageStyle;
      }
    }

    return "";
  },

  /**
   * Restore the selected style sheet of all the frames in frameList
   * to match |pageStyle|.
   * @param docShell the root docshell of all the frames
   * @param frameList a list of [frame, data] pairs, where frame is a
   * DOM window and data is the session restore data associated with
   * it.
   * @param pageStyle the title of the style sheet to apply
   */
  restore: function (docShell, frameList, pageStyle) {
    let disabled = pageStyle == NO_STYLE;

    let markupDocumentViewer =
      docShell.contentViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);
    markupDocumentViewer.authorStyleDisabled = disabled;

    for (let [frame, data] of frameList) {
      Array.forEach(frame.document.styleSheets, function(aSS) {
        aSS.disabled = aSS.title && aSS.title != pageStyle;
      });
    }
  },
};
